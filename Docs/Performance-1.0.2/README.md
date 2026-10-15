# 1.0.2 identification evidence — 2026-10-15

This checkpoint replaces the generic-fit identification approach with measured
stage ports. It does **not** certify the reduced runtime sound or promote new DSP.
See [the engineering contract](../REDUCED_ENGINE_PLAN.md).

- `reference-manifest.json`: pinned original circuit source, compiler, executable
  and source hashes, and original/instrumented trace comparison.
- `capture-validity.json`: corrected `identification-v4` dataset; complete matrix,
  data hashes, valid fractions and solver hard failures.
- `stage-identification.json`: compact measured-stage summary and candidate hash;
  includes signed gain, static-map explanation and memory observations.
- `stage-fit.txt`: per-stage static-map diagnostics; these are identification
  errors, not fresh fixture, aliasing, full-chain or listening results.
- `identification-tests.txt`: offline pipeline regression, including rejection of
  stale source DC, excessive exclusions, truncated/nonfinite data, missing models,
  and negative-gain/drive-normalization mistakes.
- `rejected-capture-drafts.json`: invalidated draft datasets and reason.
- `smoke.txt`: processor/UI/state/routing regression on the final 1.0.2 build.

Large binary fixtures, original/instrumented traces and 1,025-knot candidates are
under `build/validation/reduced-circuit-1.0.2`, excluded from version control.
Candidate extraction requires the strict capture gate to pass and never replaces
the production calibration header. The old 1.0.1 fitted previews remain experimental.

The new VST3 is a native arm64 development build, developer UI disabled and locally
ad-hoc signed. It is not universal, Developer-ID signed, notarized or installed.
No release has been published. Intel, real-DAW and program-material acceptance of
the new measured approach remain open.
