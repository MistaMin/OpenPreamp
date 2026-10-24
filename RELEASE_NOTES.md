# OpenPreamp 1.1.2

The 1.1.2 release upgrades static preamp anti-aliasing to HybridEQ's Residual ADAA architecture, eliminating high-frequency attenuation while preserving robust anti-aliasing protection.

- **HybridEQ Residual ADAA Architecture**: Replaced uncompensated two-sample averaging with a split linear/residual topology (`dsp::adaa::Residual`). Linear small-signal audio passes through a unity-gain phase-matched all-pass filter, maintaining transparent top-end energy from 20 Hz to Nyquist (< 0.005 dB difference at 20 kHz).
- **Non-Linear Residual Anti-Aliasing**: Anti-derivative anti-aliasing is applied exclusively to the non-linear distortion residual with smooth compression cross-fading under deep saturation.
- **Closed-Form & Hermite Antiderivatives**: Implemented exact mathematical antiderivatives (`legF`, `railLimitF`, `coreSaturateF`, `asymLimitF`) across Brit, N-Type, FSF, and A-Type models.
- **Alias Suppression**: Retains 4.5 dB to 7.7 dB alias foldback reduction at 1x oversampling without compromising audio brightness or clarity.
- **Universal macOS Package**: Includes Developer ID signed and Apple-notarized VST3, AU, CLAP, LV2, and PACE-signed AAX plugins in a system-wide PKG and DMG for macOS 11+. The official AAX binary is proprietary freeware.

See [user manual](Docs/UserManual.md) and [version history](CHANGELOG.md).
