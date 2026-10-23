#!/usr/bin/env python3
"""Stage only public formats, or refresh the signed package's file manifest."""
from pathlib import Path
import argparse, hashlib, json, shutil, subprocess
ROOT=Path(__file__).resolve().parents[1]
VERSION=(ROOT/'VERSION').read_text().strip()
FORMATS={'VST3':'OpenPreamp.vst3','AU':'OpenPreamp.component','CLAP':'OpenPreamp.clap','LV2':'OpenPreamp.lv2','AAX':'OpenPreamp.aaxplugin'}

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
    for fmt,name in FORMATS.items():shutil.copytree(build/'OpenPreamp_artefacts/Release'/fmt/name,out/'Plugins'/fmt/name,symlinks=True)
    licenses=out/'Licenses';licenses.mkdir()
    for src,name in [('LICENSE','LICENSE'),('Licenses/OpenPreamp-BINARY_LICENSE.txt','BINARY_LICENSE.txt'),('Licenses/OpenPreamp-THIRD_PARTY_NOTICES.txt','THIRD_PARTY_NOTICES.txt'),('third_party/GoodLookinUI/LICENSE','GoodLookinUI-LICENSE.txt')]:shutil.copy2(ROOT/src,licenses/name)
    shutil.copytree(ROOT/'Licenses/third-party',licenses/'third-party')
    shutil.copy2(ROOT/'output/pdf/OpenPreamp-User-Manual.pdf',out/'OpenPreamp-User-Manual.pdf')
    for name in ['CHANGELOG.md','RELEASE_NOTES.md']:shutil.copy2(ROOT/name,out/name)
    (out/'INSTALL.txt').write_text(f'''OpenPreamp {VERSION} - OpenGrid / Marcos Deida
Mac universal: Apple Silicon and Intel. macOS 11 or later.

These are intermediate plugin-staging files, not the public installer image.
Build the system-wide PKG and its DMG folder with:
python3 tools/build_openpreamp_installer.py --plugins <this-folder>/Plugins \
  --work <new-work-folder> --out <new-dmg-folder> \
  --application-identity '<Developer ID Application identity>' \
  --installer-identity '<Developer ID Installer identity>'

The public DMG must contain OpenPreamp-{VERSION}-Installer.pkg. Quit your DAW
and run that PKG to install into /Library/Audio/Plug-Ins for all users.
The installer then asks about matching older/current user copies, with Move to
Trash as the default; no user copies move until the user confirms.
See Docs/MAC_INSTALLER.md for inspection, notarization and publication.

This production build has no DEV editor. Project source is MIT licensed;
dependency and official binary-use terms are in Licenses/. AAX installs to
/Library/Application Support/Avid/Audio/Plug-Ins.
Source and downloads: https://github.com/MistaMin/OpenPreamp
''')
    assert (out/'Plugins/AAX/OpenPreamp.aaxplugin').is_dir(),'AAX bundle missing from staging'
    # All full license texts must be present, not just a reference to them.
    for license in (ROOT/'Licenses/third-party').iterdir():
        if license.is_file():assert (licenses/'third-party'/license.name).read_bytes()==license.read_bytes()
    print(out)
def manifest(out):
    records=[]
    for p in sorted(out.rglob('*')):
        if p.is_file() and p.name not in ['MANIFEST.json','SHA256SUMS.txt']:
            records.append({'path':str(p.relative_to(out)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size})
    assert any(x['path'].endswith('OpenPreamp.aaxplugin/Contents/MacOS/OpenPreamp') for x in records)
    data={'product':'OpenPreamp','version':VERSION,'source_commit':run('git','-C',str(ROOT),'rev-parse','HEAD'),'architectures':['arm64','x86_64'],'minimum_macos':'11.0','developer_mode':False,'formats':list(FORMATS),'excluded_formats':[],'files':records}
    (out/'MANIFEST.json').write_text(json.dumps(data,indent=2)+'\n')
    (out/'SHA256SUMS.txt').write_text(''.join(x['sha256']+'  '+x['path']+'\n' for x in records))
    print('Manifest:',len(records),'files; AAX included')
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('action',choices=['stage','manifest']);ap.add_argument('--build',type=Path,default=ROOT/'build-release');ap.add_argument('--out',type=Path,default=ROOT/f'dist/OpenPreamp-{VERSION}-macOS');args=ap.parse_args()
    if args.action=='stage':stage(args.build,args.out)
    else:manifest(args.out)
