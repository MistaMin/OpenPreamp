# OpenPreamp 1.0.3

This patch updates the preamp controls and adds inverse gain compensation.

- Off appears first in the model menu; N-Type remains the initial selection.
  Saved model identities are retained.
- ENGINE choices are Circuit and HQ. Circuit is the reduced engine; its known
  HQ fidelity differences remain documented. HQ is the factory engine.
- OS offers only 1x, 2x and 4x. Initial rates are 1x for Standard/Circuit and 2x
  for HQ; after a manual selection, mode changes preserve the chosen rate.
  Old sessions retain their legacy rate/automation mappings.
- GAIN COMPENSATION moves input/output inversely in dB, independently of L/R LINK.
  It preserves current gains when enabled and stops the pair at either gain limit.
  Compensation couples user knob edits; it is not measured auto-loudness matching.
- Mouse-wheel gain and frequency adjustment works on hovered or clicked knobs.
- MS MODE, L/R LINK and GAIN COMPENSATION form a centered header group.
- Updated screenshots and illustrated manual. Fresh 48 kHz/PCM24 measurements
  compare all four models × Standard/Circuit/HQ × explicit 1x/2x/4x, with two
  offline captures per configuration and 92 PNG/SVG chart pairs plus raw CSV.
  Gain compensation is off for fixed-setting measurements. No synthetic hardware
  plots or physical hardware measurements are included.

Read [the measurement report](Docs/Measurements-1.0.3/REPORT.md),
[control details](Docs/CONTROL_UPDATES.md) and [user manual](Docs/UserManual.md).
Linear sweep harmonic/alias maps complement the quantitative steady-tone tests;
short fixtures do not establish long recovery, listening approval or hardware
sound equivalence. Circuit has not passed every HQ sound-equivalence gate.

The universal macOS 11+ VST3, AU, CLAP and LV2 plugins ship in a Developer ID
signed, Apple-notarized PKG inside the notarized DMG. Run the PKG to install into
/Library/Audio/Plug-Ins for all users. After installation, a dialog lists matching
older/current user copies; Move to Trash is the default and still requires a
response. Newer copies and other accounts are retained. Unattended installs keep
user copies and include a review command in the installed support folder.

AAX binaries, private references and optional SDKs are excluded. Mono/stereo only.
Meters show sample peaks, not true peaks. There is no limiter. Mode changes reset
histories without a transition crossfade. Models use estimated parameters and
simplified devices. Direct host automation of one gain changes only that gain;
GUI compensation edits can record both gain parameters.
