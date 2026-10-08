#!/usr/bin/env python3
"""Stage only public formats, or refresh the signed package's file manifest."""
from pathlib import Path
import argparse, hashlib, json, shutil, subprocess
ROOT=Path(__file__).resolve().parents[1]
VERSION=(ROOT/'VERSION').read_text().strip()
FORMATS={'VST3':'OpenPreamp.vst3','AU':'OpenPreamp.component','CLAP':'OpenPreamp.clap','LV2':'OpenPreamp.lv2'}

def run(*args):return subprocess.check_output(args,text=True,stderr=subprocess.STDOUT).strip()
def binary(bundle):
    if bundle.suffix=='.lv2':return next(p for p in bundle.iterdir() if p.suffix in ('.so','.dylib'))
    return bundle/'Contents/MacOS/OpenPreamp'
def stage(build,out):
    cache=(build/'CMakeCache.txt').read_text()
    assert 'OPENPREAMP_DEVELOPER_MODE:BOOL=OFF' in cache,'Developer tools must be OFF'
    for fmt,name in FORMATS.items():
        src=build/'OpenPreamp_artefacts/Release'/fmt/name
        assert src.exists(),src
        assert set(run('lipo','-archs',str(binary(src))).split())=={'arm64','x86_64'},src
    assert not out.exists(),f'Refusing to overwrite {out}'
    assert (ROOT/'output/pdf/OpenPreamp-User-Manual.pdf').exists(),'Build manual first'
    out.mkdir(parents=True)
    for fmt,name in FORMATS.items():shutil.copytree(build/'OpenPreamp_artefacts/Release'/fmt/name,out/'Plugins'/fmt/name)
    licenses=out/'Licenses';licenses.mkdir()
    for src,name in [('LICENSE','LICENSE'),('Licenses/OpenPreamp-BINARY_LICENSE.txt','BINARY_LICENSE.txt'),('Licenses/OpenPreamp-THIRD_PARTY_NOTICES.txt','THIRD_PARTY_NOTICES.txt'),('third_party/GoodLookinUI/LICENSE','GoodLookinUI-LICENSE.txt')]:shutil.copy2(ROOT/src,licenses/name)
    shutil.copytree(ROOT/'Licenses/third-party',licenses/'third-party')
    shutil.copy2(ROOT/'output/pdf/OpenPreamp-User-Manual.pdf',out/'OpenPreamp-User-Manual.pdf')
    for name in ['CHANGELOG.md','RELEASE_NOTES.md']:shutil.copy2(ROOT/name,out/name)
    (out/'INSTALL.txt').write_text(f'''OpenPreamp {VERSION} - OpenGrid / Marcos Deida
Mac universal: Apple Silicon and Intel. macOS 11 or later.

Quit the DAW. Copy the complete bundle from the matching Plugins subfolder to:
VST3: ~/Library/Audio/Plug-Ins/VST3/OpenPreamp.vst3
AU:   ~/Library/Audio/Plug-Ins/Components/OpenPreamp.component
CLAP: ~/Library/Audio/Plug-Ins/CLAP/OpenPreamp.clap
LV2:  ~/Library/Audio/Plug-Ins/LV2/OpenPreamp.lv2
Finder: Go > Go to Folder. Create the final format folder if needed.
Restart the DAW and rescan. See the illustrated user manual for all controls.

This production build has no DEV editor. Project source is MIT licensed;
dependency and official binary-use terms are in Licenses/. AAX is excluded.
Source and downloads: https://github.com/MistaMin/OpenPreamp
''')
    assert not any(p.suffix=='.aaxplugin' for p in out.rglob('*')),'AAX must never enter public staging'
    # All full license texts must be present, not just a reference to them.
    for license in (ROOT/'Licenses/third-party').iterdir():
        if license.is_file():assert (licenses/'third-party'/license.name).read_bytes()==license.read_bytes()
    print(out)
def manifest(out):
    records=[]
    for p in sorted(out.rglob('*')):
        if p.is_file() and p.name not in ['MANIFEST.json','SHA256SUMS.txt']:
            records.append({'path':str(p.relative_to(out)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size})
    assert not any('.aaxplugin' in x['path'].lower() for x in records)
    data={'product':'OpenPreamp','version':VERSION,'source_commit':run('git','-C',str(ROOT),'rev-parse','HEAD'),'architectures':['arm64','x86_64'],'minimum_macos':'11.0','developer_mode':False,'formats':list(FORMATS),'excluded_formats':['AAX'],'files':records}
    (out/'MANIFEST.json').write_text(json.dumps(data,indent=2)+'\n')
    (out/'SHA256SUMS.txt').write_text(''.join(x['sha256']+'  '+x['path']+'\n' for x in records))
    print('Manifest:',len(records),'files; AAX excluded')
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('action',choices=['stage','manifest']);ap.add_argument('--build',type=Path,default=ROOT/'build-release');ap.add_argument('--out',type=Path,default=ROOT/f'dist/OpenPreamp-{VERSION}-macOS');args=ap.parse_args()
    if args.action=='stage':stage(args.build,args.out)
    else:manifest(args.out)
