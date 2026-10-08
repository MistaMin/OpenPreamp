#!/usr/bin/env python3
"""Verify universal architecture, metadata, license payloads and CLAP/LV2 entry points."""
from pathlib import Path
import ctypes as C, json, plistlib, subprocess, re
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'build-release/OpenPreamp_artefacts/Release';VERSION=(ROOT/'VERSION').read_text().strip()
checks=[]
for fmt,name in [('VST3','OpenPreamp.vst3'),('AU','OpenPreamp.component'),('CLAP','OpenPreamp.clap'),('LV2','OpenPreamp.lv2'),('AAX','OpenPreamp.aaxplugin')]:
    bundle=ART/fmt/name
    if fmt=='AAX' and not bundle.exists():continue
    binary=next(p for p in bundle.iterdir() if p.suffix in ('.so','.dylib')) if fmt=='LV2' else bundle/'Contents/MacOS/OpenPreamp'
    archs=set(subprocess.check_output(['lipo','-archs',str(binary)],text=True).split());assert archs=={'arm64','x86_64'},(fmt,archs)
    licenses=bundle/'Licenses' if fmt=='LV2' else bundle/'Contents/Resources/Licenses'
    for file in ['BINARY_LICENSE.txt','THIRD_PARTY_NOTICES.txt','LICENSE','GoodLookinUI-LICENSE.txt']:assert (licenses/file).exists(),(fmt,file)
    for source in (ROOT/'Licenses/third-party').iterdir():
        if source.is_file():assert (licenses/'third-party'/source.name).read_bytes()==source.read_bytes(),(fmt,source.name)
    if fmt!='LV2':
        info=plistlib.loads((bundle/'Contents/Info.plist').read_bytes());assert info['CFBundleShortVersionString']==VERSION,(fmt,info)
    checks.append(fmt+': universal + version/license payload verified')
raw=(ART/'VST3/OpenPreamp.vst3/Contents/Resources/moduleinfo.json').read_text();info=json.loads(re.sub(r',(\s*[}\]])',r'\1',raw));assert info['Name']=='OpenPreamp';assert any(c.get('Version')==VERSION for c in info['Classes'])
checks.append('VST3: generated factory manifest version verified')
class Ver(C.Structure):_fields_=[('major',C.c_uint32),('minor',C.c_uint32),('revision',C.c_uint32)]
class Entry(C.Structure):_fields_=[('version',Ver),('init',C.CFUNCTYPE(C.c_bool,C.c_char_p)),('deinit',C.CFUNCTYPE(None)),('get_factory',C.CFUNCTYPE(C.c_void_p,C.c_char_p))]
class Desc(C.Structure):_fields_=[('version_clap',Ver)]+[(key,C.c_char_p) for key in ['id','name','vendor','url','manual_url','support_url','version','description']]+[('features',C.POINTER(C.c_char_p))]
class Factory(C.Structure):_fields_=[('count',C.CFUNCTYPE(C.c_uint32,C.c_void_p)),('descriptor',C.CFUNCTYPE(C.POINTER(Desc),C.c_void_p,C.c_uint32)),('create',C.c_void_p)]
clap=ART/'CLAP/OpenPreamp.clap';lib=C.CDLL(str(clap/'Contents/MacOS/OpenPreamp'));entry=Entry.in_dll(lib,'clap_entry');assert entry.init(str(clap).encode());pointer=entry.get_factory(b'clap.plugin-factory');factory=C.cast(pointer,C.POINTER(Factory)).contents;assert factory.count(pointer)==1;desc=factory.descriptor(pointer,0).contents;assert desc.name==b'OpenPreamp' and desc.version.decode()==VERSION;entry.deinit();checks.append('CLAP: library loads, factory initializes, descriptor/version verified')
class LV2Desc(C.Structure):_fields_=[('uri',C.c_char_p)]+[(key,C.c_void_p) for key in ['instantiate','connect_port','activate','run','deactivate','cleanup','extension_data']]
lv2=ART/'LV2/OpenPreamp.lv2';binary=next(p for p in lv2.iterdir() if p.suffix in ('.so','.dylib'));lib2=C.CDLL(str(binary));lib2.lv2_descriptor.argtypes=[C.c_uint32];lib2.lv2_descriptor.restype=C.POINTER(LV2Desc);desc2=lib2.lv2_descriptor(0).contents;assert desc2.uri==b'https://github.com/MistaMin/OpenPreamp' and desc2.run and desc2.instantiate;assert binary.name in (lv2/'manifest.ttl').read_text();checks.append('LV2: library loads and descriptor/binary reference verified')
for check in checks:print('PASS',check)
(ROOT/'build-release/validation/formats.json').write_text(json.dumps({'version':VERSION,'checks':checks,'aax_distribution':False},indent=2)+'\n')
