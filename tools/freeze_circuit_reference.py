#!/usr/bin/env python3
"""Create an independently buildable original-circuit reference, without a checkout.
The trace driver is measurement-only. Its hash is retained beside the pinned DSP.
"""
import argparse,hashlib,json,platform,subprocess
from pathlib import Path

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--ref',required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];out=args.output.resolve()
    if out.exists():raise ValueError('Use a new output directory; frozen references are never overwritten')
    revision=subprocess.check_output(['git','rev-parse','--verify',args.ref+'^{commit}'],cwd=root,text=True).strip()
    out.mkdir(parents=True);hashes={}
    for name in ['CircuitSolver.h','BritCircuit.h','NTypeCircuit.h','FsfCircuit.h','ATypeCircuit.h']:
        relative='Source/DSP/'+name;data=subprocess.check_output(['git','show',revision+':'+relative],cwd=root);target=out/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);hashes[relative]=hashlib.sha256(data).hexdigest()
    relative='tests/openpreamp_reference_trace.cpp';data=(root/relative).read_bytes();target=out/relative;target.parent.mkdir(parents=True);target.write_bytes(data);hashes[relative]=hashlib.sha256(data).hexdigest()
    command=['clang++','-std=c++20','-O3','-DNDEBUG','-I',str(out/'Source/DSP'),str(target),'-o',str(out/'reference-trace')];subprocess.run(command,check=True)
    hashes['reference-trace']=hashlib.sha256((out/'reference-trace').read_bytes()).hexdigest()
    manifest={'schema':1,'circuit_source_revision':revision,'source_version':subprocess.check_output(['git','show',revision+':VERSION'],cwd=root,text=True).strip(),'compiler':subprocess.check_output(['clang++','--version'],text=True),'compile_command':command,'system':platform.platform(),'machine':platform.machine(),'sha256':hashes,'scope':'Circuit outputs only; not a full processor CPU or resampling reference.'}
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(out/'manifest.json')
if __name__=='__main__':main()
