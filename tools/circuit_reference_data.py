"""Shared offline data/filter helpers; captured signals are little-endian float64 pairs."""
from pathlib import Path
import numpy as np
from scipy.signal import lfilter
RATE=96000.0
MAIN_POLES=[.3,2.,15.,60.,250.,1000.,4000.,12000.,30000.,80000.]
def filtered(x,fc,kind='hp',rate=RATE):
    if fc==0:return x.copy()
    g=np.pi*fc/rate;a=(1-g)/(1+g)
    b=[g/(1+g)]*2 if kind=='lp' else [1/(1+g),-1/(1+g)]
    return lfilter(b,[1,-a],x,axis=0)
def response(fc,frequencies,kind='hp'):
    z=np.exp(-2j*np.pi*frequencies/RATE);g=np.pi*fc/RATE
    low=g/(1+g)*(1+z)/(1-(1-g)/(1+g)*z)
    return low if kind=='lp' else 1-low
def fixture(directory,name,kind):
    file=Path(directory)/f'{name}-{kind}.bin';xy=np.fromfile(file,dtype='<f8').reshape(-1,2)
    valid=np.fromfile(str(file)+'.valid',dtype=np.uint8).astype(bool)
    if len(valid)!=len(xy):raise ValueError('Incomplete capture/mask: '+str(file))
    return xy[:,0],xy[:,1],valid
