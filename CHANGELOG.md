# OpenPreamp version history

## 1.0.2 - development (2026-10-15)

- Measure only explicit 1x/2x/4x rates; add coherent folded-harmonic energy, alias spectra, residual plots and raw alias CSVs.
- Add shared hardware/plugin PCM measurement fixtures, calibrated loopback import, a 36-configuration preamp/mode/oversampling capture matrix, repeated-power analysis and CSV/PNG/SVG comparison reports. See Docs/MEASUREMENT_PIPELINE.md.
- Name the engines Standard (lighter), Circuit Preview (reduced) and HQ (full component model).
- Add Default/1x/2x/4x oversampling. Circuit defaults to 1x with prepared residual ADAA; Standard and Circuit disable ADAA at 2x/4x. HQ retains its 2x default.
- Append the new rate parameter while retaining existing IDs/order and the legacy 4x parameter; old sessions preserve their previous rates.
- Follow the HQ-to-Eco guide with a pinned, independently buildable circuit reference and read-only stage probes for all four models.
- Add stage impulse/drive/memory captures, strict reference-validity checks, measured 1,025-knot residual/antiderivative extraction and capture regression tests.
- Correct stale source voltage in offline DC capture setup; runtime solver behavior is retained.
- Keep Full as the default. Existing Reduced previews have not passed complete sound acceptance; new measured candidates are identification data only.
- See Docs/REDUCED_ENGINE_PLAN.md for the contract, evidence and remaining acceptance gates.

## 1.0.1 - development (2026-10-09)

CPU-reduction work in progress.

- Repeatable CPU baselines with warm-up, processor-only timing, multiple signals, rates and buffers, and buffer-deadline checks.
- Accelerated Circuit-off Brit and N-Type with prepared transfer-curve tables and conservative reference fallbacks; existing ADAA integration and histories retained.
- Waveform, aliasing and 226 processor/UI regression checks passed on native Apple Silicon. See Docs/CPU_OPTIMIZATION.md for measured results and remaining work.
- Preserve the released Circuit/HQ algorithms and session parameter identities.

## 1.0.0 - 2026-10-08

First public release of the preamp and metering tool extracted from HybridEQ.

- Brit, N-Type, FSF and A-Type preamps, plus a clean Off model.
- Full component-network Circuit mode at 2x; HQ at 4x with FIR resampling.
- Lighter session-rate models with ADAA when Circuit is off.
- Independent stereo or Mid/Side input/output trims, Link, PAD and bypass.
- Shared session-rate 12 dB/octave high/low cuts and Side-only 6 dB/octave mono maker.
- Independent meters with direct-click Input/Output and Peak/RMS buttons, clip lamps and peak bars.
- Model-dependent plates and knobs; proportional resizing down to 400 x 296.
- Mac universal VST3, AU, LV2 and CLAP package, full illustrated user manual and license notices.
- AAX build is private and excluded from downloads. Developer tools are excluded from release binaries.

## 0.3.1 - development

Always-visible dual-meter layout, linked gains, mono maker, Peak/RMS mode,
compact resizing, and developer tools for refining the saved design.

## 0.3.0 - development

Independent channel trims and meters, M/S encode/decode, shared input cuts,
and processing of the surrounding linear tools at session rate.

## 0.2.3 - development

Resizable interface, larger meters/output controls and saved design CSVs.

## 0.2.2 - development

2x Circuit / HQ 4x oversampling, lighter ADAA path and model-specific looks.

## 0.2.1 - development experiment

Experimental UI and antialiasing work; superseded by 0.2.2.

## 0.2.0 - development

Preamp-only extraction from HybridEQ, without its EQ or separate harmonics engine.
