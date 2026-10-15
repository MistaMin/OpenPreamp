#!/usr/bin/env python3
"""Measure reduced/reference trades; never converts a fidelity failure into a pass.
Uses NumPy/SciPy/Matplotlib, the native evaluator, and OpenPreampCapture data.
--require-fidelity returns nonzero until ALL declared fidelity gates pass.
"""
import argparse,csv,json,sys
from pathlib import Path
import numpy as np
from scipy.signal import resample_poly
from scipy.io.wavfile import write as wav
from fit_reduced_physical import evaluator,NAMES
from circuit_reference_data import fixture,RATE

def main():
    p=argparse.ArgumentParser();p.add_argument('reference',type=Path);p.add_argument('--coefficients',type=Path,required=True);p.add_argument('--library',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--require-fidelity',action='store_true');args=p.parse_args()
    args.output.mkdir(parents=True,exist_ok=True);models=json.loads(args.coefficients.read_text());evaluate=evaluator(args.library)
    summary={};all_rows=[];plot_data=[]
    for index,name in enumerate(NAMES):
        model=models[index];assert model['name']==name
        params=model['parameters'];weights=model['linear_weights'];results=[]
        rows=list(csv.DictReader((args.reference/f'{name}-tones.csv').open()))
        for row in rows:
            f=float(row['frequency']);amp=float(row['amplitude']);warm=int(np.ceil(max(.08,6/f)*RATE));cycles=max(4,int(np.ceil(.08*f)));n=int(np.ceil(cycles*RATE/f));omega=2*np.pi*cycles/n
            t=np.arange(-warm,n);x=amp*np.sin(omega*t);y=evaluate(x,params,weights,index)[warm:];t=np.arange(n)
            harmonics=np.array([2*np.sum(y*np.exp(-1j*k*omega*t))/n for k in range(1,6)])
            gain=20*np.log10(max(abs(harmonics[0])/amp,1e-30));phase=np.angle(1j*harmonics[0]);hd=20*np.log10(np.maximum(abs(harmonics[1:])/max(abs(harmonics[0]),1e-30),1e-15))
            r={**row,'model':name,'reduced_gain_db':gain,'reduced_phase_rad':phase,'gain_error_db':gain-float(row['gain_db']),'phase_error_rad':float(np.angle(np.exp(1j*(phase-float(row['phase_rad'])))))}
            for k in range(2,6):r[f'reduced_h{k}_dbr']=float(hd[k-2]);r[f'h{k}_error_db']=float(hd[k-2]-float(row[f'h{k}_dbr']))
            results.append(r);all_rows.append(r)
        quiet=[r for r in results if float(r['amplitude'])==.001 and int(r['hard_failures'])==0]
        driven=[r for r in results if .1<=float(r['amplitude'])<=2 and int(r['hard_failures'])==0]
        errors=[abs(r[f'h{k}_error_db']) for r in driven for k in range(2,6) if float(r[f'h{k}_dbr'])>-80]
        metrics={'quiet_max_gain_error_db':max(abs(r['gain_error_db']) for r in quiet),'quiet_max_phase_error_rad':max(abs(r['phase_error_rad']) for r in quiet),'driven_max_harmonic_error_db_above_minus80':max(errors,default=0),'harmonic_observations':len(errors),'excluded_failed_tones':sum(int(r['hard_failures'])>0 for r in results),'waveforms':{}}
        for kind in ['validation','recovery']:
            x,y,valid=fixture(args.reference,name,kind);z=evaluate(x,params,weights,index);err=z[valid]-y[valid]
            metrics['waveforms'][kind]={'residual_db':float(10*np.log10(np.mean(err*err)/max(np.mean(y[valid]**2),1e-30))),'peak_error':float(max(abs(err))),'excluded_samples':int((~valid).sum()),'total_samples':len(x)}
            # Listening exports use a high-quality common 96 -> 48 kHz conversion.
            # The captured chirp/multitone/bursts are synthetic, not program material.
            for tag,data in [('input',x),('full',y),('reduced',z)]:wav(args.output/f'{name}-{kind}-{tag}.wav',48000,resample_poly(data,1,2).astype(np.float32))
            if kind=='recovery':plot_data.append((name,x,y,z))
        metrics['gates']={'quiet_response':metrics['quiet_max_gain_error_db']<=.25 and metrics['quiet_max_phase_error_rad']<=.03,'harmonics':metrics['driven_max_harmonic_error_db_above_minus80']<=3,'held_out_waveform':metrics['waveforms']['validation']['residual_db']<=-30,'recovery':metrics['waveforms']['recovery']['residual_db']<=-30}
        metrics['fidelity_pass']=all(metrics['gates'].values());summary[name]=metrics
        print(name,json.dumps(metrics),flush=True)
    with (args.output/'tones.csv').open('w') as f:
        writer=csv.DictWriter(f,fieldnames=list(all_rows[0]));writer.writeheader();writer.writerows(all_rows)
    (args.output/'fidelity.json').write_text(json.dumps(summary,indent=2)+'\n')
    import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
    fig,axes=plt.subplots(4,2,figsize=(13,11),layout='constrained')
    for i,(name,x,y,z) in enumerate(plot_data):
        t=np.arange(len(x))/RATE;sel=(t>=.39)&(t<=.44)
        axes[i,0].plot(t[sel],y[sel],label='Full',linewidth=1.3);axes[i,0].plot(t[sel],z[sel],label='Reduced preview',linewidth=1);axes[i,0].set_title(f'{name}: 80 Hz overload');axes[i,0].legend(loc='upper right')
        axes[i,1].plot(t,y,label='Full',linewidth=.6);axes[i,1].plot(t,z,label='Reduced preview',linewidth=.6);axes[i,1].set_title(f'{name}: burst, quiet tone, DC steps and recovery')
        for ax in axes[i]:ax.set_xlabel('Seconds');ax.set_ylabel('Digital amplitude');ax.grid(alpha=.2)
    fig.savefig(args.output/'recovery.png',dpi=150);plt.close(fig)
    fig,axes=plt.subplots(4,2,figsize=(13,11),layout='constrained')
    for i,name in enumerate(NAMES):
        quiet=[r for r in all_rows if r['model']==name and float(r['amplitude'])==.001]
        axes[i,0].semilogx([float(r['frequency']) for r in quiet],[r['gain_error_db'] for r in quiet],marker='o');axes[i,0].axhline(.25,color='r',linestyle='--');axes[i,0].axhline(-.25,color='r',linestyle='--');axes[i,0].set_title(name+' quiet gain error');axes[i,0].set_ylabel('dB')
        tone=[r for r in all_rows if r['model']==name and float(r['amplitude'])==1 and abs(float(r['frequency'])-1000)<1][0]
        axes[i,1].plot(range(2,6),[float(tone[f'h{k}_dbr']) for k in range(2,6)],'o-',label='Full');axes[i,1].plot(range(2,6),[tone[f'reduced_h{k}_dbr'] for k in range(2,6)],'o-',label='Reduced preview');axes[i,1].set_title(name+' 1 kHz / unity amplitude harmonics');axes[i,1].set_ylabel('dB relative to fundamental');axes[i,1].set_xticks(range(2,6));axes[i,1].legend()
        for ax in axes[i]:ax.grid(alpha=.2)
    fig.savefig(args.output/'response-harmonics.png',dpi=150);plt.close(fig)
    return 1 if args.require_fidelity and not all(m['fidelity_pass'] for m in summary.values()) else 0
if __name__=='__main__':sys.exit(main())
