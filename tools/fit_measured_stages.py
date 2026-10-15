#!/usr/bin/env python3
"""Extract measured 1025-knot stage candidates, with residual antiderivatives.
Identification only: diagnostics never install a runtime calibration header.
A static-map fit cannot certify dynamic or full-chain equivalence.
Requires NumPy; reference validation uses the standard library.
"""
import argparse,json
from pathlib import Path
import numpy as np
from check_stage_captures import check,csv_rows,STAGES

RATE=96000.;FREQUENCY=1000.;PERIOD=96

def settled(directory,row,stage):
    data=np.fromfile(directory/(row['fixture']+'-'+stage+'.bin'),dtype='<f8').reshape(-1,2)
    mask=np.fromfile(directory/(row['fixture']+'.valid'),dtype=np.uint8).astype(bool)
    # A contiguous integer-cycle region; no interpolation across invalid samples.
    begin=int(row['analysis_begin']);longest=(0,0);start=begin
    for i in range(begin,len(mask)+1):
        if i==len(mask) or not mask[i]:
            if i-start>longest[1]-longest[0]:longest=(start,i)
            start=i+1
    count=(longest[1]-longest[0])//PERIOD*PERIOD
    if count<4608:raise ValueError('Insufficient settled valid cycles: '+row['fixture'])
    return data[longest[1]-count:longest[1]]

def fundamental(x):
    return 2*np.mean(x*np.exp(-2j*np.pi*FREQUENCY*np.arange(len(x))/RATE))

def aligned(data,phase,gain):
    # Offline phase alignment only. Negative gain is factored separately, so an
    # inverting stage does not have its even-harmonic polarity reversed by a
    # half-cycle time shift. Never normalize by the driven fundamental gain.
    frequencies=np.fft.fftfreq(len(data),1/RATE)
    y=np.fft.ifft(np.fft.fft(data[:,1])*np.exp(-1j*frequencies/FREQUENCY*phase)).real/gain
    return data[:,0],y

def curve(x,y,grid):
    # Average rising/falling branches into a candidate; keep the branch error
    # separately. A low fit error on the averaged curve is not a memory test.
    order=np.argsort(x);x=x[order];y=y[order]
    bins=np.clip(np.searchsorted(grid,x),1,len(grid)-1)
    sums=np.bincount(bins,weights=y,minlength=len(grid));counts=np.bincount(bins,minlength=len(grid))
    xs=np.bincount(bins,weights=x,minlength=len(grid));used=counts>0
    return np.interp(grid,xs[used]/counts[used],sums[used]/counts[used])

def extract(directory,output):
    validity=check(directory)
    if not validity['passed']:raise ValueError('Reference validity gate failed: '+', '.join(validity['errors']))
    fixtures=csv_rows(directory/'fixtures.csv');ports=csv_rows(directory/'ports.csv');profiles=[]
    for model in STAGES:
        rows=sorted([r for r in fixtures if r['model']==model and r['kind']=='curve'],key=lambda r:float(r['amplitude']))
        impulse=next(r for r in fixtures if r['model']==model and r['kind']=='impulse')
        stages=[p['stage'] for p in ports if p['fixture']==impulse['fixture']]
        for stage in stages:
            quiet=settled(directory,rows[0],stage);u0=fundamental(quiet[:,0]);v0=fundamental(quiet[:,1])
            if abs(u0)<1e-12:raise ValueError('Unidentifiable stage '+model+'/'+stage)
            h=v0/u0
            if abs(h)<1e-12:raise ValueError('Unidentifiable stage '+model+'/'+stage)
            sign=1 if h.real>=0 else -1;phase=float(np.angle(h/sign));gain=sign*abs(h)
            captures=[]
            for row in rows:
                data=settled(directory,row,stage);x,y=aligned(data,phase,gain);extent=float(max(abs(x)));grid=np.linspace(-extent,extent,1025);mapped=curve(x,y,grid)
                estimate=np.interp(x,grid,mapped);residual_db=float(10*np.log10(max(np.mean((y-estimate)**2),1e-30)/max(np.mean(y*y),1e-30)))
                # This includes phase-dispersion/curve interpolation errors as well
                # as hysteresis; it is not a measurement of hysteresis alone.
                captures.append({'amplitude':float(row['amplitude']),'extent':extent,'grid':grid,'mapped':mapped,'static_residual_db':residual_db,'zero_crossing_output':float(np.interp(0,grid,mapped))})
            extent=max(c['extent'] for c in captures);grid=np.linspace(-extent,extent,1025)
            combined=np.zeros_like(grid);coverage=np.zeros(len(grid),dtype=bool)
            # Largest domain supplies the extremes; smaller captures replace its
            # center using a smooth crossfade, preserving drive compression.
            for c in sorted(captures,key=lambda c:c['extent'],reverse=True):
                mapped=np.interp(grid,c['grid'],c['mapped']);limit=c['extent'];blend=np.clip((.9*limit-abs(grid))/(.1*limit),0,1)
                if not coverage.any():combined=mapped;coverage[:]=True
                else:combined+=blend*(mapped-combined)
            offset=float(np.interp(0,captures[0]['grid'],captures[0]['mapped']));combined-=offset
            center_width=max(captures[0]['extent']*.25,2*(grid[1]-grid[0]));blend=np.clip(abs(grid)/center_width,0,1);blend=blend*blend*(3-2*blend)
            residual=(combined-grid)*blend
            antiderivative=np.concatenate([[0],np.cumsum(.5*(residual[:-1]+residual[1:])*np.diff(grid))])
            impulse_data=np.fromfile(directory/(impulse['fixture']+'-'+stage+'.bin'),dtype='<f8').reshape(-1,2)
            u=np.fft.rfft(impulse_data[:,0]);v=np.fft.rfft(impulse_data[:,1]);freq=np.fft.rfftfreq(len(impulse_data),1/RATE)
            wanted=np.geomspace(20,20000,128);idx=np.unique(np.searchsorted(freq,wanted));good=abs(u[idx])>max(abs(u))*1e-7;idx=idx[good];transfer=v[idx]/u[idx]
            worst=max(c['static_residual_db'] for c in captures)
            memory_row=next(r for r in fixtures if r['model']==model and r['kind']=='memory')
            memory_data=np.fromfile(directory/(memory_row['fixture']+'-'+stage+'.bin'),dtype='<f8').reshape(-1,2)
            memory={}
            for label,begin,end in [('baseline',0,.18),('driven_bass',.3,.6),('quiet_after_overload',.8,1.1),('late_recovery',1.8,2.)]:
                region=memory_data[int(begin*RATE):int(end*RATE)]
                memory[label]={'input_mean_volts':float(np.mean(region[:,0])),'output_mean_volts':float(np.mean(region[:,1])),'input_rms_volts':float(np.sqrt(np.mean(region[:,0]**2))),'output_rms_volts':float(np.sqrt(np.mean(region[:,1]**2)))}
            profiles.append({'model':model,'stage':stage,'units':'input volts; normalized output volts before signed stage gain','signed_gain_1khz':float(gain),'phase_1khz_rad':phase,'grid':grid.tolist(),'nonlinear_residual':residual.tolist(),'residual_antiderivative':antiderivative.tolist(),'center_blend_volts':float(center_width),'input_domain_volts':[-extent,extent],'endpoint_residual_slopes':[float((residual[1]-residual[0])/(grid[1]-grid[0])),float((residual[-1]-residual[-2])/(grid[-1]-grid[-2]))],'captures':[{k:v for k,v in c.items() if k not in ('grid','mapped')} for c in captures],'worst_static_residual_db':worst,'static_explanation_pass':worst<=-30,'linear_response':{'frequency_hz':freq[idx].tolist(),'real':transfer.real.tolist(),'imag':transfer.imag.tolist()}})
            profiles[-1]['memory_observations']=memory
            print(model,stage,'worst static explanation',round(worst,2),'dB','PASS' if worst<=-30 else 'REQUIRES FURTHER IDENTIFICATION',flush=True)
    output.parent.mkdir(parents=True,exist_ok=True)
    result={'schema':1,'role':'identification-candidate','sample_rate':RATE,'knot_count':1025,'runtime_promotion_allowed':False,'note':'No fitted LTI runtime filters or validated dynamics yet. Memory observations are identification data, not a dynamics fit. Do not promote on static diagnostics alone. A fresh final holdout is still required.','reference_sha256':validity['sha256'],'profiles':profiles}
    output.write_text(json.dumps(result,indent=2,allow_nan=False)+'\n');return result
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('directory',type=Path);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();extract(args.directory,args.output)
