#!/usr/bin/env python3
"""Publish actual-plugin diagnostics and a complete chart/CSV release archive."""
import argparse
import hashlib
import json
import shutil
import zipfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--archive', type=Path, required=True)
    args = parser.parse_args()
    source, out = args.source.resolve(), args.out.resolve()
    summary = json.loads((source / 'summary.json').read_text())
    captures = summary['captures']
    expected = {(model, mode, rate) for model in ('Brit', 'N-Type', 'FSF', 'A-Type')
                for mode in ('Standard', 'Circuit', 'HQ') for rate in ('1x', '2x', '4x')}
    actual = {(c['model'], c['mode'], c['oversampling']) for c in captures}
    if actual != expected or len(captures) != 36 or any(c['kind'] != 'plugin' for c in captures):
        raise ValueError('Expected 36 actual plugin configurations and no hardware substitutes')
    pngs = sorted(source.glob('*.png'))
    if len(pngs) != 92 or any(not p.with_suffix('.svg').is_file() for p in pngs):
        raise ValueError('Expected 92 complete PNG/SVG chart pairs')
    root = str(Path(__file__).resolve().parents[1])
    def portable(value):
        if isinstance(value, dict): return {k: portable(v) for k, v in value.items()}
        if isinstance(value, list): return [portable(v) for v in value]
        if isinstance(value, str): return value.replace(root + '/', '')
        return value
    summary = portable(summary)
    summary['publication'] = {'capture_wavs_included': False,
        'note': 'Original renderer and capture hashes retained. These captures predate release packaging. All analyzed CSV bins are in the release archive.'}
    out.mkdir(parents=True, exist_ok=True)
    files = sorted(p for p in source.iterdir() if p.is_file() and
                   (p.suffix in ('.png', '.svg', '.csv') or p.name == 'index.html'))
    for path in files:
        destination = out / path.name
        shutil.copy2(path, destination)
        if destination.suffix in ('.csv', '.svg', '.html'):
            destination.write_text('\n'.join(line.rstrip() for line in destination.read_text().splitlines()) + '\n')
    (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    report = '''# OpenPreamp 1.0.2 audio measurements

Actual plugin diagnostics: **4 preamps × Standard/Circuit/HQ × explicit 1x/2x/4x**.
36 configurations, two offline captures each; 48 kHz, PCM24, 64-sample buffers,
identical 9.803-second quick fixture. No Default traces or synthetic hardware plots.
Settings, effective oversampling, ADAA policy, latency, capture hashes and analysis
versions are recorded in [summary.json](summary.json) and [configurations.csv](configurations.csv).

[Download every chart and raw CSV, including full spectrogram bins](https://github.com/MistaMin/OpenPreamp/releases/download/v1.0.2/OpenPreamp-1.0.2-Audio-Measurements.zip).
Download this directory and open [index.html](index.html) for the complete chart gallery.
The GitHub directory includes 92 PNG/SVG chart pairs and ordinary CSV tables;
large gzip CSV maps are distributed in the release archive. Original capture WAVs
are not bundled; their hashes and renderer provenance remain in the manifest.

## Reading the results

Standard and Circuit use ADAA at 1x and disable it at 2x/4x; HQ uses the full
component model without ADAA. 2x uses IIR resampling; 4x uses FIR resampling.
All plots use explicit rates. Circuit remains labeled Preview: the reduced models
have not passed every HQ sound-equivalence gate. A full product release does not
establish hardware equivalence or promote measured stage candidates into the engine.

These short diagnostics use four levels and short settling. They do not establish
a detailed knee, long recovery, listening approval or a final phase tolerance.
Repeated deterministic renders do not estimate analog noise. The input reference
shows the quantized stimulus; no physical hardware was captured.

## Comparisons

'''
    for stem, caption in [('transfer', 'Transfer and gain error'), ('distortion', 'Distortion'),
                          ('dynamics', 'Burst dynamics'), ('phase', 'Phase and group delay'),
                          ('aliasing', 'Folded harmonic energy and residual')]:
        report += f'### {caption}\n\n![{caption}](all-preamps-{stem}.png)\n\n'
    report += '''## Linear sweep harmonic and alias maps

Continuous 20 Hz–20 kHz linear chirps at −18/−6 dBFS; full 0–24 kHz output axis,
common −140…0 dBFS peak/bin scale. Periodic four-term Blackman–Harris STFT,
512 samples, hop 256, 93.75 Hz bin spacing, 187.908 Hz ENBW. Window duration
10.67 ms; fundamental sweep drift about 106.6 Hz/window (H5 about 532.8 Hz).
Window width, chirp drift and overlapping trajectories mean brightness is not an
isolated harmonic level. Use steady-tone tables for quantitative harmonic energy.
Clean maps and H1–H5/fold guides are both available in the gallery.

'''
    for model in ('brit', 'ntype', 'fsf', 'atype'):
        report += f'![{model} mode/rate maps at −6 dBFS](linear-sweep-{model}-6.png)\n\n'
    report += '''## Reproduce

See [measurement workflow](../MEASUREMENT_PIPELINE.md) for fixture generation,
offline capture, hardware calibration/import and analysis commands. The published
snapshot retains the original analysis provenance; binaries packaged later are
not represented as having produced these captures.
'''
    (out / 'REPORT.md').write_text(report)
    args.archive.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted(out.iterdir()):
            if path.is_file(): archive.write(path, 'OpenPreamp-1.0.2-Audio-Measurements/' + path.name)
        for path in sorted(source.glob('*.csv.gz')):
            archive.write(path, 'OpenPreamp-1.0.2-Audio-Measurements/' + path.name)
    digest = hashlib.sha256(args.archive.read_bytes()).hexdigest()
    print(f'{len(pngs)} chart pairs, {len(captures)} plugin configurations; archive SHA256 {digest}')


if __name__ == '__main__':
    main()
