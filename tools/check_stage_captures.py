#!/usr/bin/env python3
"""Validate identification data before fitting; incomplete/invalid data fails closed."""
import argparse,array,csv,hashlib,json,math,sys
from pathlib import Path

STAGES={'brit':5,'ntype':5,'fsf':6,'atype':4}
def csv_rows(path):
    with Path(path).open(newline='') as file:return list(csv.DictReader(file))

def check(directory):
    directory=Path(directory);fixtures=csv_rows(directory/'fixtures.csv');ports=csv_rows(directory/'ports.csv')
    result={'role':'identification','schema':1,'fixtures':[],'errors':[],'sha256':{}}
    seen=set()
    for row in fixtures:
        name=row['fixture'];model=row['model']
        if name in seen:raise ValueError('Duplicate fixture: '+name)
        if Path(name).name!=name:raise ValueError('Invalid fixture path')
        seen.add(name);frames=int(row['frames']);begin=int(row['analysis_begin'])
        if model not in STAGES or int(row['rate'])!=96000 or not 0<=begin<frames:raise ValueError('Unsupported identification metadata')
        if row['kind'] not in ('impulse','curve','memory') or row['accepted'] not in ('0','1'):raise ValueError('Unsupported fixture kind or acceptance value')
        mask_path=directory/(name+'.valid');mask=mask_path.read_bytes()
        if len(mask)!=frames or any(x not in (0,1) for x in mask):raise ValueError('Incomplete/corrupt validity mask: '+name)
        valid=sum(mask[begin:]);longest=run=0
        for flag in mask[begin:]:
            run=run+1 if flag else 0;longest=max(longest,run)
        impulse=row['kind']=='impulse';accepted=valid/(frames-begin)>=(.999 if impulse else .99) and longest>=(32768 if impulse else 4608)
        if valid!=int(row['valid_frames']) or longest!=int(row['longest_valid_run']) or accepted!=bool(int(row['accepted'])):raise ValueError('Validity summary mismatch: '+name)
        selected=[p for p in ports if p['fixture']==name]
        if len(selected)!=STAGES[model] or len({p['stage'] for p in selected})!=STAGES[model]:raise ValueError('Missing/duplicate stage ports: '+name)
        source_name='input_coupling' if model=='brit' else 'input_transformer'
        source=next((p for p in selected if p['stage']==source_name),None)
        if source is None or abs(float(source['input_dc_volts']))>1e-12:raise ValueError('Source was not reset before measuring DC: '+name)
        paths=[mask_path,directory/(name+'-complete.bin')]
        for port in selected:
            stage=port['stage']
            if Path(stage).name!=stage or port['model']!=model or not all(math.isfinite(float(port[k])) for k in ['input_dc_volts','output_dc_volts']):raise ValueError('Corrupt stage metadata: '+name)
            paths.append(directory/(name+'-'+stage+'.bin'))
        for path in paths:
            data=path.read_bytes();result['sha256'][path.name]=hashlib.sha256(data).hexdigest()
            if path.suffix=='.bin':
                if len(data)!=16*frames:raise ValueError('Incomplete sample data: '+path.name)
                values=array.array('d');values.frombytes(data)
                if sys.byteorder!='little':values.byteswap()
                if any(flag and (not math.isfinite(values[2*i]) or not math.isfinite(values[2*i+1])) for i,flag in enumerate(mask)):raise ValueError('Valid mask contains nonfinite sample: '+path.name)
        item={'fixture':name,'model':model,'kind':row['kind'],'accepted':accepted,'valid_fraction':valid/(frames-begin),'excluded_analysis_samples':frames-begin-valid,'hard_failures':int(row['hard_failures'])}
        result['fixtures'].append(item)
        if not accepted:result['errors'].append('Rejected numerical reference: '+name)
    for model in STAGES:
        actual=[r for r in fixtures if r['model']==model]
        if len(actual)!=10 or sum(r['kind']=='impulse' for r in actual)!=1 or sum(r['kind']=='memory' for r in actual)!=1 or sorted(float(r['amplitude']) for r in actual if r['kind']=='curve')!=[.001,.03,.1,.3,.5,1,2,4]:result['errors'].append('Incomplete requested fixture matrix: '+model)
    if {p['fixture'] for p in ports}!=seen:result['errors'].append('Orphan stage port metadata')
    for name in ['fixtures.csv','ports.csv']:result['sha256'][name]=hashlib.sha256((directory/name).read_bytes()).hexdigest()
    result['passed']=not result['errors'];return result
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('directory',type=Path);parser.add_argument('--report',type=Path,required=True);args=parser.parse_args()
    try:report=check(args.directory)
    except (OSError,ValueError,KeyError,OverflowError) as exc:report={'passed':False,'errors':[str(exc)]}
    args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('sha256','fixtures')}));raise SystemExit(0 if report['passed'] else 1)
