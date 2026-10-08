# OpenPreamp handoff — 2026-10-08

## Requested scope

Extract HybridEQ's preamp and metering. No EQ, spectrum display, separate
harmonics engine or harmonics panel in the OpenPreamp product.
The original HANDOFF.md is historical HybridEQ context; it is not the current
OpenPreamp handoff. The legacy HybridEQ target and sources remain in this
checkout, but are not used by the OpenPreamp processor/editor.

## Findings and changes

- The previous 420 × 280 editor had only preamp/output panels and no meters.
  It now has a fixed 860 × 330 console layout: PAD, drive, model, circuit,
  bypass, output trim, VU, stereo input/output peak ladders, output peak hold,
  clip/reset and a session-rate readout. Bypass dims preamp controls.
- The processor previously used preampPad's choice index as its oversampling
  index. Per the owner's final instruction, OpenPreamp now has **no oversampling**
  parameter, resampler or latency-padding stage. PAD changes drive only.
- The circuit always runs at the session rate, with decimation disabled.
  A 44.1/48/96/192 kHz session runs the circuit at exactly that rate.
  The editor displays SESSION RATE; reported latency is zero.
- Version reset from the inherited HybridEQ 1.6.8 numbering to OpenPreamp 0.2.0 (pre-release).
- Declared support for matched mono/stereo input and output buses.
- Hidden knob rendering now uses the actual parameter position so exported
  editor snapshots show the correct knob angle before animation has run.
- MeterPanel now accepts input/output LevelTracker references, with a delegating
  processor constructor. It no longer includes or depends on HybridEQProcessor.
- Corrected the OpenPreamp CPU benchmark to measure session-rate processing
  while keeping PAD at unity. Moved the test option ahead of the OpenPreamp test targets, so
  they exist on fresh configurations.

## Build outputs

Build directory: build, Unix Makefiles, Release.

- build/OpenPreamp_artefacts/Release/Standalone/OpenPreamp.app
- build/OpenPreamp_artefacts/Release/VST3/OpenPreamp.vst3
- build/OpenPreamp_artefacts/Release/AU/OpenPreamp.component

These are freshly built workspace artifacts. No system installation was done.
The inherited installer scripts still package HybridEQ; do not use those to
install OpenPreamp until their names, paths and bundle identifiers are updated.
CLAP/LV2 remain configured targets but were not rebuilt in this repair.

## Verification

OpenPreampSmoke tests the actual processor and editor: PAD/rate independence,
all four models across 44.1/48/96/192 kHz with circuit off/on,
finite output, stable reported latency, a bypass impulse with output gain,
input/output meter taps, state round-trip, mono bus support, GUI containment,
VU/peak/clip ballistics and reset. It also renders the real editor to PNG.

Results: **81 checks, 0 failures** in Release for version 0.2.0. Preview:
build/OpenPreamp-0.2.0-preview.png. Standalone, VST3, AU and CPU-table targets built successfully. No DAW/audio-device listening test or full CPU
benchmark was performed. Existing inherited compiler warnings remain.

## Remaining limitations

Circuit CPU cost was not optimized or benchmarked in this repair. At high session rates, these component-level models can be costly.
Without oversampling, nonlinear distortion can alias into the audible band;
this follows the owner's explicit request for session-rate-only processing.
The inherited behavioral op-amp models, A-Type discrete-op-amp placeholder,
FSF output-transformer placeholders and estimated netlist values are unchanged. The separate harmonics engine is excluded; naturally
occurring preamp distortion is part of the retained preamp model.

Circuit explanation: [CIRCUIT_EXPLAINED.md](CIRCUIT_EXPLAINED.md).
