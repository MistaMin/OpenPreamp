# Measurement pipeline verification — 2026-10-16

Development diagnostics, not a hardware or sound-equivalence acceptance report.
The source workflow is documented in `Docs/MEASUREMENT_PIPELINE.md`.

- The 48 kHz/PCM24 matrix includes all four preamps × Standard/Circuit/HQ ×
  explicit 1x/2x/4x: **36 configurations, 72 independent offline renders**.
  Each uses the same 9.803-second quick fixture and a fixed 64-sample buffer.
  Every rate/ADAA combination was checked against the expected policy; no
  clipped samples were observed. The report exports **92 PNG/SVG chart pairs**,
  including folded-harmonic energy at two drive levels, a high-frequency alias
  spectrum, nonharmonic residual and a cross-model aliasing overview.
  Two continuous 20 Hz → 20 kHz linear chirps at −18/−6 dBFS add 18 clean,
  annotated and input-reference map charts. Maps use a common −140…0 dBFS
  scale, full Nyquist output range, and explicit FFT/ENBW/chirp-smearing captions.
  The periodic four-term Blackman–Harris STFT uses N=512, hop=256 for this quick
  capture. Full map bins are exported to 37 indexed gzip CSVs (36 DUTs + input).
- The earlier 96 kHz/PCM16 check covers Brit Circuit/HQ at explicit 1x/2x/4x:
  **6 configurations, 12 renders**, fixed 257-sample buffer. This validates the
  second supported rate, another PCM width and non-power-of-two buffering; it
  is not a complete 96 kHz matrix and has no dedicated alias fixtures.
- **24 numerical/validation tests passed**: PCM16/24/32 round trips, known gain,
  phase and harmonic amplitudes, THD+N/bandwidth, unavailable harmonics, preserved
  polarity/group delay, lock-in gain, noise-power averaging, stimulus integrity,
  calibration/clock requirements, format/truncation and loopback de-embedding.
  Alias checks include analytic cubic folding, a folded fifth harmonic at 96 kHz,
  a clean linear reference and exclusion of real in-band harmonics.
  A coherent 9 kHz regression rejects artificial fold energy from window leakage.
  Eight new sweep tests cover chirp cycles/level, absolute peak-bin units,
  power averaging, predicted multiple folds, an actual cubic-generated alias,
  time/frequency coordinates, 96 kHz Nyquist coverage and full gzip CSV export.
- Hardware import and loopback de-embedding retain internal numerical/integration
  coverage. Synthetic hardware plots are excluded from the delivered report;
  only actual plugin captures are presented here.
- Representative charts were visually inspected: cross-model transfer grid,
  mode/rate gain deviation, burst dynamics, spectrum and phase difference.

Full local reports/raw WAVs/CSVs remain under:

```text
build/measurements/48k-final-report/index.html
build/measurements/48k-linear-matrix/captures.json
build/measurements/96k-explicit-report/index.html
build/measurements/96k-subset/captures.json
```

Compact configurations, tone data, noise and representative overview plots are
retained here. Full manifests contain settings, effective factor/ADAA, all host
parameters, hashes, latency and analysis versions. These quick fixtures use only
four levels and short settling; they cannot establish a detailed knee, long
recovery, full overload behavior or a final phase/fidelity tolerance. All inspected
captures are development data. No physical hardware, listening signoff, live DAW
measurement, release, installation or push was performed.
Chirp maps show diagnostic spectral content; finite window width, sweep drift,
fades and overlapping trajectories prevent interpreting their brightness as an
isolated harmonic level. Steady-tone tests retain the quantitative measurements.
The copied linear-sweep index uses filenames relative to the full local report
directory above; the large CSV.gz map files remain in that report directory.
Older Default rows are omitted when importing old manifests. Previous raw
captures/reports remain available as historical development evidence; they do not
add redundant traces to current measurement plots.
