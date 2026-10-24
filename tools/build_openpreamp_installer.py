#!/usr/bin/env python3
"""Build a system-wide OpenPreamp product PKG and a DMG staging folder.

Existing signed plugin bundles are reused without recompiling DSP. Signing is
optional for local inspection; release builds must supply both identities.
"""
import argparse
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[1]
FORMATS = {'VST3': ('Library/Audio/Plug-Ins/VST3', 'OpenPreamp.vst3'),
           'AU': ('Library/Audio/Plug-Ins/Components', 'OpenPreamp.component'),
           'CLAP': ('Library/Audio/Plug-Ins/CLAP', 'OpenPreamp.clap'),
           'LV2': ('Library/Audio/Plug-Ins/LV2', 'OpenPreamp.lv2'),
           'AAX': ('Library/Application Support/Avid/Audio/Plug-Ins', 'OpenPreamp.aaxplugin'),
           'Standalone': ('Applications', 'OpenPreamp.app')}
SUPPORT = Path('Library/Application Support/OpenGrid/OpenPreamp')


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def build(args):
    version = (ROOT / 'VERSION').read_text().strip()
    if args.work.exists() or args.out.exists():
        raise ValueError('Use new work/output directories; existing packages are never overwritten')
    for fmt, (_, name) in FORMATS.items():
        source = args.plugins / fmt / name
        binary = source / 'libOpenPreamp.so' if fmt == 'LV2' else source / 'Contents/MacOS/OpenPreamp'
        architectures = subprocess.check_output(['lipo', '-archs', str(binary)], text=True).split()
        if set(architectures) != {'arm64', 'x86_64'}:
            raise ValueError(f'{fmt} must be universal')
        if fmt != 'LV2':
            info = plistlib.loads((source / 'Contents/Info.plist').read_bytes())
            if info['CFBundleShortVersionString'] != version:
                raise ValueError(f'{fmt} version mismatch')
    args.work.mkdir(parents=True)
    payload = args.work / 'root'
    support = payload / SUPPORT
    support.mkdir(parents=True)
    for fmt, (folder, name) in FORMATS.items():
        destination = payload / folder / name
        shutil.copytree(args.plugins / fmt / name, destination, symlinks=True)
    for source, name in [('output/pdf/OpenPreamp-User-Manual.pdf', 'OpenPreamp-User-Manual.pdf'),
                         ('CHANGELOG.md', 'CHANGELOG.md'), ('RELEASE_NOTES.md', 'RELEASE_NOTES.md')]:
        shutil.copy2(ROOT / source, support / name)
    shutil.copytree(args.plugins.parent / 'Licenses', support / 'Licenses')
    helper = support / 'InstallerSupport/OpenPreampUserCopies'
    helper.parent.mkdir()
    run('clang++', '-std=c++17', '-fobjc-arc', '-arch', 'arm64', '-arch', 'x86_64',
        '-mmacosx-version-min=11.0', f'-DOPENPREAMP_INSTALL_VERSION="{version}"',
        '-framework', 'AppKit', ROOT / 'installer/openpreamp/UserCopies.mm', '-o', helper)
    run(helper, '--self-test')
    run('/usr/bin/arch', '-x86_64', helper, '--self-test')
    if args.application_identity:
        run('codesign', '--force', '--timestamp', '--options', 'runtime',
            '--sign', args.application_identity, helper)
    review = support / 'Check User Copies.command'
    review.write_text('#!/bin/bash\nexec "/Library/Application Support/OpenGrid/OpenPreamp/InstallerSupport/OpenPreampUserCopies"\n')
    review.chmod(0o755)
    scripts = args.work / 'scripts'
    scripts.mkdir()
    shutil.copy2(ROOT / 'installer/openpreamp/postinstall', scripts / 'postinstall')
    (scripts / 'postinstall').chmod(0o755)
    components = args.work / 'components.plist'
    run('pkgbuild', '--analyze', '--root', payload, components)
    bundles = plistlib.loads(components.read_bytes())
    for bundle in bundles:
        bundle['BundleIsRelocatable'] = False
        bundle['BundleIsVersionChecked'] = True
        bundle['BundleOverwriteAction'] = 'upgrade'
    components.write_bytes(plistlib.dumps(bundles))
    component = args.work / 'OpenPreampPayload.pkg'
    run('pkgbuild', '--root', payload, '--component-plist', components, '--scripts', scripts,
        '--identifier', 'com.opengrid.openpreamp.system', '--version', version,
        '--ownership', 'recommended', '--install-location', '/', component)
    resources = args.work / 'resources'
    resources.mkdir()
    welcome = f'''<html><body><h1>OpenPreamp {escape(version)}</h1>
<p>Quit your audio hosts before continuing. This installer requires administrator authorization.</p>
<p>Installs universal VST3, Audio Unit, CLAP and LV2 plugins for all users into
<b>/Library/Audio/Plug-Ins</b>, and AAX into
<b>/Library/Application Support/Avid/Audio/Plug-Ins</b>, and the standalone app into
<b>/Applications</b>. macOS 11 or later.</p>
<p>After installation, existing OpenPreamp copies in the logged-in user's plugin
folders are listed in a separate dialog. <b>Move to Trash</b> is the default.
Only choosing <b>Move to Trash</b> authorizes cleanup; newer versions are kept.
Other accounts and renamed comparison plugins are left alone.</p>
<p>Unattended installs retain all user copies. Use Check User Copies.command in
/Library/Application Support/OpenGrid/OpenPreamp to review them later.</p></body></html>'''
    (resources / 'welcome.html').write_text(welcome)
    (resources / 'conclusion.html').write_text('''<html><body><h2>OpenPreamp installed</h2>
<p>Restart your DAW and rescan plugins. Manual, licenses and the optional user-copy
check are in /Library/Application Support/OpenGrid/OpenPreamp.</p>
<p>If you kept user copies, your DAW may load those instead of the system version.
Close your DAW and run Check User Copies.command to review them later.</p></body></html>''')
    shutil.copy2(ROOT / 'Licenses/OpenPreamp-BINARY_LICENSE.txt', resources / 'license.txt')
    distribution = args.work / 'Distribution.xml'
    distribution.write_text(f'''<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>OpenPreamp {escape(version)}</title>
  <options customize="never" rootVolumeOnly="true" hostArchitectures="arm64,x86_64"/>
  <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
  <volume-check><allowed-os-versions><os-version min="11.0"/></allowed-os-versions></volume-check>
  <welcome file="welcome.html" mime-type="text/html"/>
  <license file="license.txt" mime-type="text/plain"/>
  <conclusion file="conclusion.html" mime-type="text/html"/>
  <choices-outline><line choice="plugins"/></choices-outline>
  <choice id="plugins" title="OpenPreamp system plugins" description="VST3, AU, CLAP, LV2, AAX and Standalone" visible="false"><pkg-ref id="com.opengrid.openpreamp.system"/></choice>
  <pkg-ref id="com.opengrid.openpreamp.system" version="{escape(version)}" onConclusion="none">OpenPreampPayload.pkg</pkg-ref>
</installer-gui-script>
''')
    args.out.mkdir(parents=True)
    package = args.out / f'OpenPreamp-{version}-Installer.pkg'
    command = ['productbuild', '--distribution', distribution, '--resources', resources,
               '--package-path', args.work]
    if args.installer_identity:
        command += ['--sign', args.installer_identity, '--timestamp']
    run(*command, package)
    shutil.copy2(ROOT / 'output/pdf/OpenPreamp-User-Manual.pdf', args.out / 'OpenPreamp-User-Manual.pdf')
    shutil.copytree(support / 'Licenses', args.out / 'Licenses')
    (args.out / 'INSTALL.txt').write_text(f'''OpenPreamp {version} — system-wide macOS installer

Quit your DAW. Open OpenPreamp-{version}-Installer.pkg and follow Installer.
Administrator authorization is required. Plugins are installed here:
/Library/Audio/Plug-Ins/VST3/OpenPreamp.vst3
/Library/Audio/Plug-Ins/Components/OpenPreamp.component
/Library/Audio/Plug-Ins/CLAP/OpenPreamp.clap
/Library/Audio/Plug-Ins/LV2/OpenPreamp.lv2
/Library/Application Support/Avid/Audio/Plug-Ins/OpenPreamp.aaxplugin
/Applications/OpenPreamp.app

After installation, the logged-in user's matching older/current copies are
listed. Move to Trash is the default; Move to Trash requires your consent.
Newer versions, renamed comparison plugins and other users' files are retained.
Unattended installation retains user copies. Review them later by opening:
/Library/Application Support/OpenGrid/OpenPreamp/Check User Copies.command

Restart your DAW and rescan. The manual and licenses are included and installed
in /Library/Application Support/OpenGrid/OpenPreamp. AAX is included for Pro Tools.
''')
    print(package)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plugins', type=Path, required=True, help='Signed staging Plugins directory')
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True, help='New folder to wrap in a DMG')
    parser.add_argument('--application-identity')
    parser.add_argument('--installer-identity')
    arguments = parser.parse_args()
    arguments.plugins = arguments.plugins.resolve()
    arguments.work = arguments.work.resolve()
    arguments.out = arguments.out.resolve()
    build(arguments)
