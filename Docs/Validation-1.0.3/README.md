# OpenPreamp 1.0.3 release validation

Universal Release build, macOS 11 minimum, developer mode OFF. Both arm64 and
x86_64 (Rosetta on this Apple Silicon Mac) pass 32 focused controls, 555 processor/UI,
58 native ADAA and 336 reduced-kernel checks, plus the ADAA alias test. Each passes
960 realtime callbacks without C++ heap allocations or frees. The fresh AU factory
instantiates and renders finite stereo audio at 48 kHz/128 samples on both architectures.

The public VST3, AU, CLAP and LV2 formats have current metadata, universal binaries,
complete license payloads and strictly verified Developer ID signatures. CLAP and
LV2 entry points load; VST3 factory metadata is verified. AAX is excluded.

The installer inspection verifies exact signed payload bytes in the system plugin
folders, disabled home installation and bundle relocation, and ten fixture checks
per architecture for consent, identity, version and symlink handling. No live install
or installed user-plugin cleanup was performed. Move to Trash remains the default
button and requires the user to respond; unattended installs retain copies.

See the attached logs and JSON. The release validation asset also records final
package notarization, source revision and package digests. This is automated and
visual verification; no new DAW listening approval or physical hardware capture
is claimed. Fresh plugin audio diagnostics are in ../Measurements-1.0.3/REPORT.md.
