#!/usr/bin/env python3
"""Inspect an expanded product PKG without installing or touching user plugins."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
FORMATS = {'VST3': ('VST3', 'OpenPreamp.vst3'), 'AU': ('Components', 'OpenPreamp.component'),
           'CLAP': ('CLAP', 'OpenPreamp.clap'), 'LV2': ('LV2', 'OpenPreamp.lv2')}


def verify(expanded, plugins):
    component = expanded / 'OpenPreampPayload.pkg'
    payload = component / 'Payload'
    info = ET.parse(component / 'PackageInfo').getroot()
    assert info.get('install-location') == '/' and info.get('auth') == 'root'
    assert info.get('relocatable') == 'false'
    distribution = ET.parse(expanded / 'Distribution').getroot()
    domains = distribution.find('domains')
    assert domains.get('enable_currentUserHome') == 'false'
    assert domains.get('enable_localSystem') == 'true' and domains.get('enable_anywhere') == 'false'
    assert distribution.find('options').get('rootVolumeOnly') == 'true'
    assert distribution.find('volume-check/allowed-os-versions/os-version').get('min') == '11.0'
    assert not (payload / 'Users').exists()
    assert not any('.aaxplugin' in str(path).lower() for path in payload.rglob('*'))
    checks = ['System-volume-only installation; home install and bundle relocation disabled; no AAX']
    for fmt, (folder, name) in FORMATS.items():
        installed = payload / 'Library/Audio/Plug-Ins' / folder / name
        original = plugins / fmt / name
        assert installed.is_dir(), installed
        files = {p.relative_to(original) for p in original.rglob('*') if p.is_file()}
        delivered = {p.relative_to(installed) for p in installed.rglob('*') if p.is_file()}
        assert files == delivered, fmt
        for file in files:
            assert hashlib.sha256((original / file).read_bytes()).digest() == hashlib.sha256((installed / file).read_bytes()).digest(), (fmt, file)
        target = installed / 'libOpenPreamp.so' if fmt == 'LV2' else installed
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(target)], check=True)
        checks.append(f'{fmt}: exact signed bundle preserved at /Library/Audio/Plug-Ins/{folder}/{name}')
    scripts = component / 'Scripts'
    assert (scripts / 'postinstall').read_bytes() == (ROOT / 'installer/openpreamp/postinstall').read_bytes()
    subprocess.run(['bash', '-n', str(scripts / 'postinstall')], check=True)
    support = payload / 'Library/Application Support/OpenGrid/OpenPreamp'
    helper = support / 'InstallerSupport/OpenPreampUserCopies'
    assert set(subprocess.check_output(['lipo', '-archs', str(helper)], text=True).split()) == {'arm64', 'x86_64'}
    subprocess.run(['codesign', '--verify', '--strict', str(helper)], check=True)
    for architecture in ('arm64', 'x86_64'):
        result = subprocess.check_output(['/usr/bin/arch', '-' + architecture, str(helper), '--self-test'], text=True)
        assert '10 checks, 0 failures' in result
        checks.append(f'{architecture}: 10 consent/version/identity/symlink cleanup checks passed')
    for file in ('OpenPreamp-User-Manual.pdf', 'Licenses/LICENSE', 'Check User Copies.command'):
        assert (support / file).is_file(), file
    checks.append('Manual, licenses, user review command and exact postinstall script present')
    return {'version': (ROOT / 'VERSION').read_text().strip(), 'live_install_performed': False,
            'default_cleanup_button': 'Move to Trash', 'requires_user_response': True,
            'unattended_cleanup': False, 'checks': checks}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expanded', type=Path, required=True)
    parser.add_argument('--plugins', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.expanded, args.plugins)
    args.out.write_text(json.dumps(result, indent=2) + '\n')
    for check in result['checks']:
        print('PASS', check)
