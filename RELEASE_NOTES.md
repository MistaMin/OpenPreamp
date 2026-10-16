# OpenPreamp 1.0.2

Full source release with Standard, Circuit Preview and HQ engines across Brit,
N-Type, FSF and A-Type. HQ selects the full component model and remains the
factory default. Circuit Preview provides reduced models with documented fidelity
limits; measured stage-identification candidates are not promoted into the engine.

- Independent Default / 1x / 2x / 4x oversampling. Default selects 1x for
  Standard/Circuit and 2x for HQ; legacy sessions retain their previous rates.
- Native-rate ADAA for Standard/Circuit, disabled at 2x/4x. HQ uses no ADAA.
  2x uses IIR resampling and 4x uses FIR resampling.
- Published measurement pipeline and actual 48 kHz/PCM24 plugin report:
  four models × three engines × three explicit rates, 72 offline captures,
  92 PNG/SVG chart pairs, complete CSV data and linear sweep harmonic/alias maps.
- Updated illustrated manual and screenshots, plus reproducible CPU, residual
  ADAA, realtime and measurement checks. Windows build/installer source included.

Read [the measurement report](Docs/Measurements-1.0.2/REPORT.md) and
[user manual](Docs/UserManual.md). The release measurement ZIP includes the
complete gallery and gzip CSV spectrogram maps. Synthetic hardware plots are
excluded; no physical hardware was measured. Short fixtures do not establish
listening approval, detailed dynamics or hardware equivalence.

Signed, notarized universal macOS VST3, AU, CLAP and LV2 packages will be added
to this release when ready. Source publication takes priority. AAX binaries,
private references and optional SDKs are excluded from public distribution.

Mono/stereo only. Meters show sample peaks, not true peaks. Mode changes reset
histories without a transition crossfade. There is no limiter. Models contain
estimated parameters and simplified devices. Circuit Preview has not passed
all HQ sound-equivalence gates.
