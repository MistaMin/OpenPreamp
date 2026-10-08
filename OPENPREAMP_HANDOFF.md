# OpenPreamp 0.2.3 UI/developer handoff — 2026-10-08

0.2.3 keeps the 0.2.2 DSP unchanged. The portrait GUI opens at 600 × 840 (5:7),
resizes proportionally, has a larger VU/output knob and a slightly smaller main
knob. Buttons are narrower. The output knob label clears the panel divider.

DEV opens separate GoodLookinUI knob/layout and model-look tools. Geometry and
looks autosave to the OpenPreamp-specific Designs CSVs and are baked into builds.
The standalone is the editing artifact. The saved-design 0.2.3 VST3 is built
with developer mode OFF; it replaces the installed 0.2.2. The 0.2.0 comparison
remains alongside it.
The 0.2.0 comparison has a separate name and class IDs; its DSP/UI source came
from c4eda25. Both temporary installed copies must be removed when the owner
says testing is finished. The receipts are build/OpenPreamp-test-install.json
and build/OpenPreamp-0.2.0-comparison-install.json. Check hashes before removal.

Validation: 157 smoke checks passed with zero failures, including proportional
resizing at 450 × 630, 600 × 840 and 800 × 1120, model looks and the DEV window.
GUI and developer-window previews are saved in Docs/. The standalone built
successfully. The saved-design Release VST3 built successfully. With developer mode OFF,
156 smoke checks passed, with zero failures, including all model looks and
resizing. Both CSVs were verified against the baked design data. The test
installation is 0.2.3; its removal remains deferred until the owner finishes testing.

Saved design changes: output trim at (90, 50); Brit magenta/Brit knobs;
N-Type Mic knobs with Midnight faceplate/Navy plates; FSF Navy faceplate;
A-Type Granite faceplate. The DSP remains unchanged.

The processing notes below describe the inherited 0.2.2 DSP and its validation.

# Inherited 0.2.2 notes

## Scope and baseline

The baseline is 0.2.0, commit c4eda25. The 0.2.1 portrait/native-only experiment
was abandoned at the owner's request. 0.2.2 keeps the preamp and metering only;
legacy HybridEQ code/targets remain separate and are not in the OpenPreamp path.

## Processing

- New stable parameter hqMode, default false. CIRCUIT on with an active colored
  preamp runs at 2x; HQ selects 4x. Circuit off, model Off and bypass use 1x.
  HQ is retained in state while inactive, but does not affect the circuit-off rate.
- Circuit decimation is disabled: the solver follows the full selected 2x/4x
  rate, including at high session rates. PAD changes drive only.
- 2x: JUCE maximum-quality polyphase half-band IIR. 4x: two maximum-quality
  linear-phase half-band equiripple FIR stages. See README for stopband targets.
- Fixed host latency is the rounded-up 4x filter delay. A preallocated delay
  pads shorter paths. Latency is reported only in prepareToPlay and remains
  stable through parameter automation. Mode changes reset filter history;
  no crossfade was added.
- ADAA is enabled only in OpenPreamp's lighter model path. Shared PreampEngine
  defaults it off, so existing HybridEQ behavior is retained. The full circuit
  equations are unchanged. ADAA uses analytic antiderivatives for tanh,
  asymmetric polynomial and smooth transformer curves; eight-point Gaussian
  integration evaluates the interval average for implicit gain/clip mappings.
  Each nonlinear stage/channel owns history; preparation/model/rate changes
  reset it. Near-equal inputs use the midpoint limit to avoid cancellation.
- ADAA reduces aliasing rather than eliminating it. It changes fractional phase
  and the high-frequency response. No inverse compensation filter was added.

## Interface

500 × 800, exactly 5:8. Top VU with Input/Output selection; output trim and HQ
button at the bottom. The meter keeps both input/output peak ladders visible.
VU, peak hold and clip lamp follow the selected source (default Output).
The meterSource parameter and hqMode save with the host state.

HybridEQ's baked Designs/LookTriggers.csv entries are reused exactly for model
plate, knob style and color. Both gain knobs and plates follow the chosen model.
Editor background colors are instance-local; they do not mutate Theme globals
or recolor other instances. Brit = Granite/console, N-Type = Marine/A,
FSF = Royal/FS, Off = Graphite/Snk, A-Type = Black/A.

## Verification and artifacts

Release VST3 built successfully with version 0.2.2 and an ad-hoc signature.
The smoke suite passed 153 checks with zero failures, covering mono/stereo,
rate selection, fixed reported latency, bypass timing, state restoration,
meter source, 5:8 layout and all five model looks.

The ADAA suite passed with zero failures. In its focused sine test at both
44.1 and 48 kHz, alias-energy reduction was Brit 13.50 dB, N-Type 26.48 dB,
FSF 29.83 dB and A-Type 5.91 dB.

The spectral test compares actual circuit-off preamp output with ADAA enabled
and disabled, at +12 dB drive, using a coherent high-frequency sine. It excludes
DC, the fundamental and the in-band second harmonic, then measures remaining
alias energy relative to the fundamental. This is a focused test, not a claim
that every possible signal gets the same reduction.

The VST3 target is the 0.2.2 delivery. Other format artifacts may still contain
older code and must be rebuilt before use. No DAW/audio-device listening test
or full CPU benchmark was performed. Existing inherited compiler warnings remain.

## Temporary test installation

The owner authorized a temporary VST3 in:
/Users/marcosdeida/Library/Audio/Plug-Ins/VST3/OpenPreamp.vst3

Remove this test copy **when the owner says testing is finished**. Do not remove
it merely because Plugin Doctor closes. build/OpenPreamp-test-install.json
records the installed version, source commit and executable SHA-256; verify
ownership before replacing/removing. No release/system-wide install is intended.

## Remaining limitations

Circuit CPU cost is unchanged and grows with the internal sample rate. Behavioral
op-amp models, the A-Type discrete-op-amp placeholder, FSF output-transformer
placeholders and estimated netlist values remain. Preamp-generated harmonics are
retained; the separate harmonic engine is excluded.
