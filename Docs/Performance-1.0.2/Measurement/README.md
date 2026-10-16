# Measurement pipeline verification — 2026-10-16

Development diagnostics, not a hardware or sound-equivalence acceptance report.
The source workflow is documented in `Docs/MEASUREMENT_PIPELINE.md`.

- The 48 kHz/PCM24 matrix includes all four preamps × Standard/Circuit/HQ ×
  explicit 1x/2x/4x: **36 configurations, 72 independent offline renders**.
  Each uses the same 5.403-second quick fixture and a fixed 64-sample buffer.
  Every rate/ADAA combination was checked against the expected policy; no
  clipped samples were observed. The report exports **74 PNG/SVG chart pairs**,
  including folded-harmonic energy at two drive levels, a high-frequency alias
  spectrum, nonharmonic residual and a cross-model aliasing overview.
- The earlier 96 kHz/PCM16 check covers Brit Circuit/HQ at explicit 1x/2x/4x:
  **6 configurations, 12 renders**, fixed 257-sample buffer. This validates the
  second supported rate, another PCM width and non-power-of-two buffering; it
  is not a complete 96 kHz matrix and has no dedicated alias fixtures.
- **16 numerical/validation tests passed**: PCM16/24/32 round trips, known gain,
  phase and harmonic amplitudes, THD+N/bandwidth, unavailable harmonics, preserved
  polarity/group delay, lock-in gain, noise-power averaging, stimulus integrity,
  calibration/clock requirements, format/truncation and loopback de-embedding.
  Alias checks include analytic cubic folding, a folded fifth harmonic at 96 kHz,
  a clean linear reference and exclusion of real in-band harmonics.
  A coherent 9 kHz regression rejects artificial fold energy from window leakage.
- A separate **synthetic** hardware-import integration run passed with two
  loopback, two DUT and two plugin stand-in captures. It exports 19 PNG/SVG
  pairs and checks gain/THD+N/phase target CSV results. These are generated test
  data, not recordings of real hardware.
- Representative charts were visually inspected: cross-model transfer grid,
  mode/rate gain deviation, burst dynamics, spectrum and phase difference.

Full local reports/raw WAVs/CSVs remain under:

```text
build/measurements/48k-final-report/index.html
build/measurements/48k-alias-matrix/captures.json
build/measurements/96k-explicit-report/index.html
build/measurements/96k-subset/captures.json
build/measurements/synthetic-alias-report-check/report/index.html
```

Compact configurations, tone data, noise and representative overview plots are
retained here. Full manifests contain settings, effective factor/ADAA, all host
parameters, hashes, latency and analysis versions. These quick fixtures use only
four levels and short settling; they cannot establish a detailed knee, long
recovery, full overload behavior or a final phase/fidelity tolerance. All inspected
captures are development data. No physical hardware, listening signoff, live DAW
measurement, release, installation or push was performed.
Older Default rows are omitted when importing old manifests. Previous raw
captures/reports remain available as historical development evidence; they do not
add redundant traces to current measurement plots.
