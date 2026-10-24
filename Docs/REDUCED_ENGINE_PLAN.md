# OpenPreamp 1.0.2: measured reduced engine

Development checkpoint, 2026-10-15. This follows the supplied
`HQ_TO_ECO_ENGINEERING_GUIDE.md`. Version 1.0.2 is not a release or a sound
acceptance decision. Full remains the default; Reduced Preview remains experimental.

Current product names: **Standard** is the existing native-rate lighter model;
**Circuit** is the reduced model (currently labeled Circuit Preview); **HQ** is
the full component model and remains the default. Earlier reports use Full/Reduced
for HQ/Circuit. The **OS** selector offers Default, 1x, 2x and 4x for all engines.
Default selects Standard/Circuit at 1x and HQ at 2x. Standard and Circuit use
ADAA only at 1x; their ADAA is disabled at 2x/4x. HQ has no added ADAA.
The old `hqMode` ID remains available as Legacy 4x Oversampling. It affects
Default only. Explicit rate selection overrides it. Old sessions missing the
new appended `preampOversampling` parameter retain their previous 2x/4x modeled
rates via a saved `legacyRateDefaults` property. Standard retains its native rate.
MODELED off selects Standard; MODELED on uses the ENGINE choice. The status line
shows the active engine and rate. These are naming changes, not new sound models.

## Contract

| Area | Requirement |
|---|---|
| Reference | Original circuit source `2c870dece509292cbf7a0c20a18d09a264ceb603` (1.0.0), independently buildable and hashed. Processor CPU comparisons need a separately pinned processor executable. |
| Model | Preserve model polarity, ordinary gain, asymmetric saturation, loading and relevant recovery. Existing generic two-stage fits are development experiments. |
| Performance | Target at least 10x against original Circuit 2x across a declared stereo 48/96 kHz, 32/64/256-frame matrix; 20x stretch. Target <=1.5% processing/audio-duration at 48 kHz/64 for the normal reduced setting. Report actual factors and callback tails. |
| Compatibility | Preserve existing parameter IDs/order, saved-state meaning, routing and reported latency. Old projects restore Full. No automatic promotion of candidate calibration. |
| Runtime | No reduced circuit solves, callback allocation/free, locks or extra audio threads. Prepare banks and retire them outside callbacks. |
| Antialiasing | Evaluate first-order ADAA on the measured nonlinear residual with an exact prepared primitive. Validate 1x/2x/4x separately before choosing a new default. |
| Acceptance | Separate capture validity, fit diagnostics, complete-chain sound, runtime safety, CPU, listening and real-host results. No test substitutes for the others. |

The Circuit runtime now defaults to native rate with first-order residual ADAA
on its two procedural clip curves. A prepared 2,049-knot warped table integrates
the exact piecewise-linear interpolant. Processing uses `average(f) + (x-old)/2`,
which applies ADAA to `f(x)-x` while preserving the quiet identity path. Histories
are per channel; tables are prepared outside callbacks. Constant curve extension
defines endpoint primitives. A safety clamp at four times the curve endpoint
outputs bounds extreme residual-ADAA overshoot. Stateful transformer ports retain
their passive implicit updates. The existing 2x/4x kernel path is retained.

This uses the existing generic calibration, not the new measured stage candidates.
Full circuits are still prepared for switching. Measured-stage reconstruction,
independent engine lifecycle and complete sound acceptance remain unfinished.
See `Performance-1.0.2/Native-ADAA/README.md` for this change's validation.

## Reference and identification implemented in 1.0.2

`tools/freeze_circuit_reference.py` extracts the original five circuit headers from
the pinned Git revision and freezes the measurement driver, compiler command and
SHA-256 hashes beside an independent executable. It never overwrites a reference.
The driver compares 131,072 float64 outputs across four models and two effective
rates (96/192 kHz), with quiet, driven, overloaded and recovery input segments.
This checks circuit output preservation, not complete processor/host equivalence.

`OpenPreampStageCapture` measures 20 actual ports with the surrounding circuit and
its loading intact:

| Model | Measured stages |
|---|---|
| Brit | Input coupling; input pair; instrumentation pair; difference amplifier; line driver |
| N-Type | Input transformer; feedback preamp; interstage coupling; class-A driver; output transformer |
| FSF | Input transformer; input loading; gain stage; interstage coupling; class-AB driver; output transformer |
| A-Type | Input transformer; gain stage; output coupling; output transformer |

At nominal 96 kHz and unity internal gain, each model receives a 65,536-frame
0.001 impulse, eight 32,768-frame 1 kHz drive captures (0.001 through 4), and a
192,000-frame bass-burst/quiet-tone/DC-step/recovery capture. INPUT/PAD remain
external gains. Source full-scale volts are 1.7/2.0/0.8/0.6 respectively.

Each stage stores little-endian float64 input/output volts with nominal DC removed;
`ports.csv` preserves both DC values. `*-complete.bin` stores original digital
input/output. Shared `*.valid` masks flag solver hard failures/nonfinite values and
the following 50 ms. Successful source-step recoveries are counted separately.

The capture tool explicitly zeros driven fixed sources **before** restoring DC
histories. Earlier drafts retained a previous fixture's source voltage in DC
metadata; those captures are invalidated. The validator rejects stale source DC.
No change to the runtime solver reset behavior is implied.

`check_stage_captures.py` rejects missing ports/fixtures, corrupt/truncated data,
nonfinite samples marked valid, inconsistent validity counts and stale source DC.
Impulse analysis requires >=99.9% valid samples and 32,768 contiguous frames;
curves/memory require >=99% valid samples and 4,608 contiguous frames. Curves use
the final 12,288 frames. DC preparation failure aborts capture.

`fit_measured_stages.py` extracts 1,025-knot candidate residuals and their
piecewise-linear antiderivatives, preserving signed small-signal gain rather than
normalizing driven gain away. It records complex linear response, static-map
explanation errors and per-stage memory observations. Negative gain is factored
separately from phase compensation, preserving even-harmonic polarity.

The generated JSON is identification data. It explicitly forbids runtime promotion.
Its static explanation diagnostic includes interpolation and phase dispersion;
it is not a pure hysteresis measurement or full-chain fidelity certification.

## Remaining gates, in order

1. Fit stable low-order linear sections per stage; report magnitude/phase error
   across the supported rates. Identification target: 0.1 dB / 1 degree in-band.
2. Reconstruct the stage cascade with measured residual maps. Use the bass and DC
   traces to identify transformer/bias/charging states, particularly N-Type.
3. Apply the implemented residual ADAA to the measured-stage reconstruction once
   calibrated. Revalidate cancellation, overload and aliasing for that cascade;
   current native-ADAA results cover the generic preview only.
4. Test the complete processor on fresh frequency/drive/IMD/recovery fixtures.
   Initial targets: driven fundamental error <=0.5 dB, harmonics 2-5 within 3 dB
   where reference harmonics exceed -80 dBr, and moderate matched-rate waveform
   residual <=-30 dB. Report severe overload and oversampling-phase differences
   separately; do not normalize away objective gain errors.
5. Treat all inspected 1.0.1 validation/recovery files as **development** data.
   Reserve a newly generated final holdout after model selection; opening or using
   it to revise a fit consumes it. Keep identification, development and final sets
   in separate directories with provenance/hashes.
6. Complete processor allocation **and free**/retirement checks, engine transitions,
   32/257-frame partition tests, original-processor compatibility, independent
   playback/offline-render lifecycle and latency validation.
7. Benchmark alternating reference/candidate runs with hashed binaries, longer
   warmup, the complete matrix and a multi-instance harness. Current quick timing
   results are not production capacity guarantees.
8. Marcos accepts the listening tradeoff on private program material, followed by
   real-DAW multi-instance/device tests and architecture/format release checks.

## Reproduce this identification checkpoint

```sh
python3 tools/freeze_circuit_reference.py --ref 2c870dece509292cbf7a0c20a18d09a264ceb603 --output build/reference-fresh
build/reference-fresh/reference-trace build/reference-fresh/traces
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DOPENPREAMP_DEVELOPER_MODE=OFF
cmake --build build --target OpenPreampStageCapture OpenPreampReferenceTrace OpenPreampSmoke -j 6
build/OpenPreampReferenceTrace build/instrumented-traces
build/OpenPreampStageCapture build/stage-identification-fresh
python3 tools/check_stage_captures.py build/stage-identification-fresh --report build/stage-validity.json
python3 tools/fit_measured_stages.py build/stage-identification-fresh --output build/stage-candidates.json
python3 -m unittest discover -s tests -p test_stage_identification.py
```

The fitter/tests require NumPy; the freezer/validator use the standard library.
Use a new capture/reference directory for each run. Compare original/instrumented
trace bytes before promoting instrumentation. Raw local evidence is under
`build/validation/reduced-circuit-1.0.2`; compact reports are in
`Docs/Performance-1.0.2`. Confidential netlists and recordings remain outside
public evidence. This checkpoint did not publish AAX. The current macOS package includes the signed AAX binary as proprietary freeware. No installation, publishing, signing for release or default-engine
change is part of this checkpoint.
