#!/usr/bin/env python3
"""Fit a fixed reduced topology with two nonlinear, stateful transformer ports.
Independent validation and recovery fixtures are not used for fitting/selection.
Build tools/reduced_circuit_fit.cpp as a shared library first. NumPy/SciPy required.
"""
import argparse,csv,ctypes,json
from pathlib import Path
import numpy as np
from scipy.optimize import least_squares
from circuit_reference_data import fixture,response,MAIN_POLES,RATE

NAMES=['brit','ntype','fsf','atype']
def linear_fit(rows,p):
    freq=np.array([float(r['frequency']) for r in rows]);target=np.array([10**(float(r['gain_db'])/20)*np.exp(1j*float(r['phase_rad'])) for r in rows])
    h=response(p[0],freq)*response(p[2],freq)*response(p[9],freq)
    a=np.column_stack([response(fc,freq)*h for fc in MAIN_POLES]);a=np.concatenate([a.real,a.imag]);target=np.concatenate([target.real,target.imag])
    return np.linalg.lstsq(np.concatenate([a,.002*np.eye(len(MAIN_POLES))]),np.concatenate([target,np.zeros(len(MAIN_POLES))]),rcond=None)[0]

def evaluator(library):
    module=ctypes.CDLL(str(library.resolve()));fn=module.openpreamp_reduced_fit
    arr=np.ctypeslib.ndpointer(dtype=np.float64,ndim=1,flags='C_CONTIGUOUS');fn.argtypes=[arr,arr,ctypes.c_int,ctypes.c_double,arr,arr,ctypes.c_int];fn.restype=None
    def evaluate(x,p,weights,model,rate=RATE):
        if len(p)==12:p=np.concatenate([p,[-30.,1.]])
        if len(p)==14:p=np.concatenate([p,[1.5,np.log(.02)]])
        x=np.ascontiguousarray(x,dtype=np.float64);p=np.ascontiguousarray(p,dtype=np.float64);weights=np.ascontiguousarray(weights,dtype=np.float64);out=np.empty_like(x)
        fn(x,out,len(x),rate,p,weights,model);return out
    return evaluate

def fit(directory,name,evaluate,initial=None):
    index=NAMES.index(name)
    rows=[r for r in csv.DictReader((directory/f'{name}-tones.csv').open()) if float(r['amplitude'])==.001 and int(r['hard_failures'])==0]
    x,y,valid=fixture(directory,name,'train');segment=int(RATE*.25);segments=len(x)//segment
    if len(x)%segment:raise ValueError('Training capture must use complete .25 second segments')
    selected=np.flatnonzero(valid & (np.arange(len(x))%32==0) & (np.arange(len(x))>4800))
    rms=np.sqrt(np.mean(y.reshape(segments,-1)**2,axis=1));weight=1/(.15+rms[np.minimum(segments-1,selected//segment)])
    p=np.array([{'brit':6.,'ntype':16.,'fsf':3.,'atype':15.}[name],np.log({'brit':.008,'ntype':.025,'fsf':.006,'atype':.008}[name]),1.5,1.6,4.,.02 if name=='ntype' else 0.,1.4,4.,0.,1.,np.log(.02 if name=='ntype' else .1),.7 if name=='ntype' else 0.,np.log(150.) if name=='ntype' else -30.,.8,1.5,np.log(.02)])
    if name=='ntype':p[3:9]=[3.5,4.0,0.0,1.7,4.0,-.35]
    if initial is not None:
        p=np.asarray(initial['parameters']);p=np.concatenate([p,[np.log(150.),.8,1.5,np.log(.02)]]) if len(p)==12 else np.concatenate([p,[1.5,np.log(.02)]]) if len(p)==14 else p
    lo=np.array([.1,np.log(.0001),.1,.15,1.,-.5,.15,1.,-.5,.1,np.log(.0001),-1.2,np.log(.001),.1,.5,np.log(.002)])
    hi=np.array([40.,np.log(1.),40.,8.,8.,.5,8.,8.,.5,30.,np.log(2.),1.2,np.log(100000.),4.,4.,np.log(.1)])
    # Physical priors prevent the fit inventing strong HF core saturation or
    # asymmetric rails in nominally balanced models to explain reference faults.
    ranges={'brit':(0.1,12.,.001,1.),'ntype':(8.,25.,.008,.08),'fsf':(.5,8.,.002,.025),'atype':(5.,25.,.003,.025)}
    fmin,fmax,kmin,kmax=ranges[name];lo[0]=fmin;hi[0]=fmax;lo[1]=np.log(kmin);hi[1]=np.log(kmax)
    lo[10]=np.log(.005);hi[10]=np.log(.5);hi[9]=10.
    if name=='ntype':lo[5]=lo[8]=-.85;hi[5]=hi[8]=.85
    else:lo[5]=lo[8]=-.03;hi[5]=hi[8]=.03;lo[11]=-.05;hi[11]=.05
    p=np.minimum(hi-1e-8,np.maximum(lo+1e-8,p))
    active=list(range(16)) if index==1 else list(range(12)) if index else [0,2,3,4,5,6,7,8,9]
    fixed=p.copy();calls=0
    def expand(v):
        full=fixed.copy();full[active]=v;return full
    def residual(v):
        nonlocal calls
        full=expand(v);weights=linear_fit(rows,full);estimate=evaluate(x,full,weights,index)
        loss=(estimate[selected]-y[selected])*weight;calls+=1
        if calls%40==0:print(name,'calls',calls,'weighted_rms',float(np.sqrt(np.mean(loss*loss))),flush=True)
        return loss
    result=least_squares(residual,p[active],bounds=(lo[active],hi[active]),max_nfev=100,diff_step=.003,ftol=1e-6,xtol=1e-6,gtol=1e-6)
    p=expand(result.x);weights=linear_fit(rows,p)
    model={'name':name,'kind':'reduced-transformer-two-stage','parameters':p.tolist(),'poles':MAIN_POLES,'linear_weights':weights.tolist(),'cost':float(result.cost),'success':bool(result.success),'nfev':int(result.nfev),'validation':{}}
    for kind in ['train','validation','recovery']:
        fx,fy,mask=fixture(directory,name,kind);estimate=evaluate(fx,p,weights,index);error=estimate[mask]-fy[mask]
        model['validation'][kind]={'valid_samples':int(mask.sum()),'invalid_samples':int((~mask).sum()),'relative_error_db':float(10*np.log10(np.mean(error*error)/max(1e-30,np.mean(fy[mask]**2)))),'peak_error':float(np.max(np.abs(error))),'peak_output':float(np.max(np.abs(estimate))),'peak_reference':float(np.max(np.abs(fy[mask])))}
    print(name,'parameters',p,'validation',model['validation'],flush=True);return model

def header(models,path):
    lines=['#pragma once','#include <array>','namespace dsp::reduced_data {','// Generated by tools/fit_reduced_physical.py from offline public circuit captures.','struct Model { std::array<double,16> parameters; std::array<double,10> weights; };','inline constexpr std::array<Model,4> models = {{']
    for m in models:lines.append('    Model{{'+', '.join(format(x,'.17g') for x in m['parameters'])+'}, {'+', '.join(format(x,'.17g') for x in m['linear_weights'])+'}},')
    lines+=['}};','}'];path.write_text('\n'.join(lines)+'\n')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('directory',type=Path);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--library',type=Path,required=True);parser.add_argument('--header',type=Path);parser.add_argument('--model',choices=NAMES);parser.add_argument('--initial',type=Path);args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True);models=[];evaluate=evaluator(args.library)
    for name in ([args.model] if args.model else NAMES):
        initial=json.loads((args.initial/(name+'.json')).read_text()) if args.initial else None
        model=fit(args.directory,name,evaluate,initial);models.append(model);(args.output/(name+'.json')).write_text(json.dumps(model,indent=2)+'\n')
    if args.header:
        if len(models)!=4:raise ValueError('Header requires all models')
        header(models,args.header)
