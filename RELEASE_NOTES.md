# OpenPreamp 1.3.0

The 1.3.0 release adds audio-clocked VU, RMS and PPM detectors with K-12/K-14/K-20 scales and optional A/B/C/D/K RMS weighting, and ports the verified ArcVu VU ballistics. The audio path, parameter IDs and saved sessions are unchanged.

- **Detector modes**: MODE selects VU, RMS, K-12, K-14, K-20, DIN, Nordic, BBC, EBU or sample Peak for both meters. Measurements run at the session sample rate; closing the editor or changing its refresh rate does not change detector timing.
- **IEC 268-17 VU ballistics**: a second-order movement (zeta 0.82, omega_n 14 rad/s) reaches 99% in about 300 ms with 1 to 1.5% overswing. 0 VU stays at a steady 1 kHz sine with a -18 dBFS peak.
- **RMS +3 and weighting**: RMS +3 shifts the RMS scale by 3.0103 dB without dropping the detector history. WEIGHT offers None, A, B, C, D or K for RMS and K scales.
- **Legacy compatibility**: old sessions restore MODE to Legacy, which keeps the original Peak/RMS behavior. New meter controls are appended to the parameter list and save with the host state.
- **macOS bundle archive**: Developer ID signed and Apple-notarized VST3, AU, CLAP, LV2 and PACE-signed AAX in a plugin-manager bundle archive for macOS 11+. The official AAX binary is proprietary freeware.

See [user manual](Docs/UserManual.md), [metering reference](Docs/METERING.md) and [version history](CHANGELOG.md).
