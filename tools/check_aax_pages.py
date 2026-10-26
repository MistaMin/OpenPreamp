#!/usr/bin/env python3
"""Check OpenPreamp's AAX control mappings against the processor parameter IDs."""
from pathlib import Path
import argparse
import re
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--bundle', type=Path, help='Also check the built AAX resource copy')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = root / 'Resources/OpenPreampPages.xml'
parameters = re.findall(r'ParameterID\s*\{\s*"([^"]+)"',
                         (root / 'Source/OpenPreampPlugin/PluginProcessor.cpp').read_text())
assert len(parameters) == len(set(parameters)), 'Duplicate processor parameter ID'
expected = set(parameters) | {'MasterBypassID'}
xml = ET.parse(source).getroot()
variants = xml.findall('./PageTableLayouts/Plugin')
assert {p.get('plugID') for p in variants} == {'jcbb', 'jccc', 'jybb', 'jycc'}
assert all(p.get('manID') == 'OPre' and p.get('prodID') == 'OpPr' for p in variants)
for kind in ('PgTL', 'MkTL', 'Av18'):
    table = xml.find(f'.//PageTable[@type="{kind}"]')
    assert table is not None, f'Missing {kind} table'
    ids = [n.text for n in table.findall('./Page/ID')]
    ids += [c.get('knobID') for c in table.findall('./Page/Cell')]
    ids = [i for i in ids if i and i.strip()]
    assert len(ids) == len(set(ids)), f'Duplicate control in {kind}'
    assert set(ids) == expected, f'{kind}: missing {expected-set(ids)}, unknown {set(ids)-expected}'
    print(f'PASS {kind}: all {len(parameters)} processor controls plus master bypass mapped')
if args.bundle:
    installed = args.bundle / 'Contents/Resources/OpenPreampPages.xml'
    assert installed.read_bytes() == source.read_bytes(), 'Built AAX pages are missing or stale'
    print('PASS built AAX page resource matches source')
