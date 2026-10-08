# OpenPreamp 1.0.0 release handoff - 2026-10-08

Production universal Mac build: arm64 + x86_64, macOS 11+, developer mode OFF.
VST3, AU, LV2 and CLAP are the public formats. AAX built successfully but is
private, outside public staging and Git history. Parameter IDs and plug-in
identities are retained from development versions.

Validation: 226 smoke/routing/UI checks passed on Apple Silicon and Intel via
Rosetta; ADAA tests passed on both architectures. AU validation passed on both
architectures after refreshing component discovery. Universal architecture,
version and all bundled licenses were verified in every built format; CLAP
factory and LV2 descriptor libraries loaded successfully; VST3 factory manifest
version was verified. These checks are not a claim of a listening test in every DAW.

Docs/UserManual.md and output/pdf/OpenPreamp-User-Manual.pdf cover every control,
track layout, circuit topology, Circuit-off ADAA, 2x Circuit and 4x HQ, resampling,
latency, installation and licensing. The 13-page PDF was rendered and visually
reviewed. README uses the new production screenshots; CHANGELOG.md records the
version history. Docs/RELEASE_PROCESS.md describes the complete release workflow.

Public staging: dist/OpenPreamp-1.0.0-macOS, explicitly only four public formats,
manual, release notes, installation instructions and complete license payloads.
Developer ID signing and Apple notarization are performed on the staged public
files. The release manifest records the frozen source commit and file hashes.
No AAX binary, SDK or private Reference content is uploaded.

The existing 0.2.0 comparison remains installed. Test plug-ins owned by this task
are removed only when the user explicitly says testing is finished. Receipts in
build/ record each installation and binary SHA-256; check them before replacement
or removal. The 1.0 AU was installed for validation and has its own receipt.

## Development history

# OpenPreamp 0.3.1 stereo-tools update — 2026-10-08

Adds the requested VUMT-style gain linking and mono maker to the approved dual
layout. Version remains 0.3.1. Minimum editor size is now 400 × 296 (default
1000 × 740, maximum 1500 × 1110). MODE and VU keys with two choices toggle
directly on click; PAD and MODEL keep dropdowns. PAD/MODEL controls were widened
and CIRCUIT/HQ narrowed. Filter DSP is unchanged: the reported low-end boost
was a phase-response display mistaken for a magnitude plot. VUMTdeluxe was tested interactively: Link disables
the right trim in L/R; M/S hides Link and enables independent Mid/Side trims;
mono-maker power greys its controls. The reference settings were restored.
VUMT also provides an Amount control; OpenPreamp implements the specifically
requested Side high-pass rather than adding an unrequested width amount.

New parameters: channelLink (off), monoMakerEnabled (off), monoMakerFrequency
(20 Hz, range 20–500 Hz), meterMode (Peak initially, RMS alternative). Existing parameter IDs are retained. Linked L/R input
and output gains both use the left values without overwriting the stored right
values. Both right knobs are disabled and dimmed. MS MODE greys out and disables Link and ignores
its gain routing even if its stored parameter remains on. Mono hosts disable
stereo-only controls. Old sessions restore new features off at 20 Hz.

Mono maker is a one-pole bilinear-transform IIR high-pass on Side, -3 dB at
cutoff and 6 dB/octave below it. Mid is unchanged. In L/R it temporarily
encodes, filters Side, and decodes before cuts/input trims; in M/S it filters
the already encoded Side. All operations and 10 ms cutoff smoothing run at
session rate, before the preamp upsampling. Preamp bypass skips it; model Off
still permits filtering. Input meters remain before this filter.

Peak/RMS buttons beside each meter source selector share meterMode. Both
needles and numeric readouts switch together; RMS uses mean-square amplitude,
Peak uses block sample peaks and the peak-hold readout. Clip lamps and LED
ladders remain peak-based. Meter choice persists in host state and does not
alter audio. Legacy HybridEQ meters retain their original display behavior.

The header holds MS MODE (off = normal L/R) and Link; the bottom row holds cuts, mono maker,
and preamp settings. Input Cuts and Preamp both measure 356 × 164; Preamp
was narrowed to match Input Cuts, with the mono maker centered between them. Saved colours, gain-knob geometry and resize limits are
retained. Current UI-test Release VST3 built with developer mode ON and passed strict
signature verification. All 227 smoke/routing checks passed, including opening
the DEV knob/layout and model-look window: linked gains, restoration
on unlink, Link visible but disabled and dimmed in M/S, disabled right controls, Side-only 6 dB/octave
response and Mid preservation at 44.1/48/96 kHz, every model against manually
encoded/Side-filtered audio at native/2x/4x, state migration, Peak/RMS needle and numeric behavior, peak clips in RMS,
shared meter-mode restore, direct-click two-option meter controls, and GUI
containment down to 400 × 296.
The test VST3 replaces the prior 0.3.1 layout build; the 0.2.0 comparison remains
untouched. The previous developer build is backed up in build/OpenPreamp-0.3.1-before-compact-ui-test-backup.vst3. Remove test installs only
when the owner explicitly says testing is finished.

For UI refinement, click DEV in the header. The separate window contains
Knobs / layout (four channel input/output knobs) and Model looks tabs. Changes
apply immediately and autosave to Designs/OpenPreampKnobs.csv and
Designs/OpenPreampLooks.csv. Developer preview is Docs/OpenPreamp-0.3.1-developer.png.
The installed bundle remains OpenPreamp.vst3, version 0.3.1; its install receipt
records developer_mode=true. Reload the plug-in to see the new build.

## Earlier 0.3.1 layout handoff

# OpenPreamp 0.3.1 layout handoff — 2026-10-08

Based on 0.3.0 commit 29366fa. This is a UI/version change; processing and
parameter IDs remain unchanged. The open VUMTdeluxe window was inspected as
an arrangement reference. Saved preamp colours and styles remain OpenPreamp's.

The editor is always dual-channel: 1000 × 740, resizable from 750 × 555 through
1500 × 1110. Both VUs are permanently visible at the top. Left/Right (or Mid/Side)
input/output trims share two aligned channel panels underneath. The bottom row
has shared input cuts and a separate preamp panel for PAD, MODEL, CIRCUIT, HQ,
M/S and bypass. The expansion arrow is removed; old expanded=false state is
ignored by the UI. Input knob labels read INPUT, matching paired OUTPUT controls.

Knob geometry in Designs/OpenPreampKnobs.csv is updated for the new channel
panels. OpenPreampLooks.csv is unchanged. Developer tools continue editing four
channel knobs in these panels; normal test VST3 builds keep developer mode OFF.

Verification: Release VST3 built with developer mode OFF. All 196 smoke and
routing checks passed, including both meters always visible, old folded-state
restore, containment at minimum/default/maximum sizes, all model looks, channel
isolation and M/S routing. Final preview is Docs/OpenPreamp-0.3.1-preview.png.
No DSP source changed. The verified test VST3 replaces 0.3.0. The separate 0.2.0 comparison is retained. Temporary
copies are removed only when the owner says testing is finished.

The following notes are historical context for the unchanged 0.3.0 DSP.

# OpenPreamp 0.3.0 handoff — 2026-10-08

The baseline is saved-design 0.2.3, commit 834665b. `>` expands the same editor
from 600 × 840 to 1200 × 840, preserving proportional resize and expansion in
host state. The extra strip duplicates input/output knobs and meter appearance.
Model, PAD, CIRCUIT, HQ, meter source and input cuts are shared. Channel trims
are independent even while the second strip is folded away.

The six new parameters are preampGainR, outputGainR, midSide, highPass, lowPass
and cutsEnabled. Defaults: Right gains 0 dB, M/S off, HP 20 Hz, LP 20 kHz,
CUTS on. Existing parameter IDs remain stable. Restoring a pre-0.3 state copies
its stereo gain values to the new right knobs and disables the new cuts.
GAIN changes now act as session-rate input trims instead of altering circuit
feedback/gain settings; this is a deliberate gain-staging change.

Processing: optional orthonormal M/S encoder → shared-frequency 12 dB/octave
Butterworth HP/LP, per-channel state → independent input gains/PAD → native
ADAA or 2x/4x full circuits → downsample → fixed latency padding → independent
output trims → optional M/S decoder. Every added linear processor runs at the
host rate. Each colored preamp owns its own circuit and ADAA state.

The dual VUs independently follow their stream (Left/Right or Mid/Side), with
input taps before cuts/gain and output taps before M/S decoding. The editor
reads level accumulators once and feeds both meters; hidden Right stays current.
Legacy HybridEQ MeterPanel defaults are preserved. Mono ignores M/S.

Validation: Release VST3 built successfully with developer mode OFF. The smoke
and routing suite passed 197 checks, zero failures: channel trims/isolation,
independent VU ballistics, 12 dB/octave cuts at 44.1/48/96 kHz, native/2x/4x
sample-rate placement, every model against a manual M/S encode/decode reference,
legacy state migration, resize, expansion and state restoration. Folded/expanded
previews are in Docs/. No Plugin Doctor or listening session was performed. The existing mode-switch behavior still resets
oversampling filters without a crossfade. M/S routing changes at block boundaries.

Temporary test installs remain owned by this task. Update OpenPreamp.vst3 to
0.3.0 after verification; keep OpenPreamp 0.2.0.vst3 alongside it. Preserve a
backup of the previous 0.2.3 bundle in build/. Remove both installed copies
only when the owner explicitly says testing is finished. Check the receipts'
SHA-256 hashes before replacing/removing.

The historical DSP notes below describe inherited models and filter choices;
0.3.0 routing and gain staging are defined above and in CIRCUIT_EXPLAINED.md.

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
