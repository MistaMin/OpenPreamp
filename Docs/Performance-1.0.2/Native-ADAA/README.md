# Native Circuit ADAA — 2026-10-15

OpenPreamp 1.0.2 development, Release arm64, developer UI off. Circuit Preview
now defaults to 1x with first-order residual ADAA. Standard remains 1x with ADAA.
Both disable ADAA at 2x/4x. HQ defaults to 2x and has no added ADAA. OS offers
Default/1x/2x/4x; previous sessions retain their rate policy until an explicit
selection overrides it. Parameter IDs/order are preserved with one new appended
parameter. The legacy 4x parameter remains available to existing automation.

Circuit's two procedural clip curves have prepared 2,049-knot tables and exact
piecewise-linear primitives. Per-channel history caches the previous evaluation.
The identity path bypasses averaging: `y = average(f) + (x-old)/2`, equivalent to
ADAA of the residual `f(x)-x`. Constant endpoint extension and a safety clamp at
four times endpoint outputs bound extreme discontinuities. Stateful transformer
updates retain the existing implicit method. This is the generic preview model;
the 20 newly identified measured stage maps have not been promoted into runtime.

Validation:

- 555 processor, state, routing and UI checks passed, including all four models,
  three engines, four rate choices at 48/96 kHz, fixed latency, old-state migration
  and actual native Circuit samples against the ADAA kernel.
- 58 native ADAA checks passed: primitive accuracy, cancellation/endpoints,
  quiet near-Nyquist response, extreme drive/recovery and arbitrary partitions
  at 44.1/48/96/192 kHz. Quiet gain changes stay below 0.02 dB and phase below
  0.002 radians in the declared fixture.
- Synthetic folded-energy reductions at 44.1/48 kHz were 6.1–19.3 dB versus
  the native generic kernel without ADAA. This is one driven high-frequency
  tone (bin 1501/8192, amplitude 2.8), not a guarantee for all signals or HQ fidelity.
- Existing reduced-kernel 336 checks, Standard ADAA tests and 144 light-table
  waveform cases plus path/fallback checks passed. Worst light-table peak error
  remains 0.0000191033, relative residual -97.29 dB.
- 960 rate/model/parameter-switch callbacks had zero C++ heap allocations or
  frees. Direct C malloc/free calls are not intercepted by this harness.
- All 16 two-second HQ/Circuit 2x/4x exports are byte-identical to the pinned
  pre-change processor export, with the same input fixture. Hashes are recorded.
- UI snapshots were rendered and inspected; the rate/ADAA status fits.
- VST3 version 1.0.2, arm64, strict local ad-hoc signature verification passed
  after manifest generation. No installation or release was performed.

Quick CPU measurements, stereo 48 kHz/64 frames, three repetitions of 0.5 seconds
after 50 ms warmup, driven tone: Brit 0.4040%, N-Type 0.6818%, FSF 0.6790%,
A-Type 0.6893% (processing wall time / audio duration). No deadline overruns
occurred in these eight silence/tone configurations. Scheduler/core unpinned;
these are local sanity timings, not a complete capacity or multi-instance study.

Reproduce with the CMake targets `OpenPreampSmoke`, `OpenPreampNativeADAA`,
`OpenPreampReduced`, `OpenPreampADAA`, `OpenPreampLightTables` and
`OpenPreampRealtime`. CPU command:

```sh
build/OpenPreampCPU_artefacts/Release/OpenPreampCPU --quick --mode 1 --factor 1 --seconds 0.5 --repeats 3
```

Full logs and raw exports remain in `build/validation/native-adaa-1.0.2`.
Compact results and executable hashes are alongside this file. Circuit remains
experimental: complete HQ fidelity, measured-stage reconstruction, listening,
real-DAW transitions and other architectures/formats remain separate gates.
