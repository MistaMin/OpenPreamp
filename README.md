# OpenPreamp

Preamp-only audio plug-in extracted from HybridEQ: Brit, N-Type, FSF and A-Type
preamp models, input PAD, drive, circuit switch, bypass, output trim and metering.
The OpenPreamp audio path contains no EQ or separate harmonics engine.

The editor includes an output VU meter (0 VU = -18 dBFS), stereo input/output peak
ladders, an output peak hold and a clip lamp. Click the clip lamp to reset it;
double-click either gain knob to return to 0 dB.

Version: **0.2.0**. The preamp circuit always runs at the DAW session sample
rate. There is no oversampling, resampling or circuit decimation in OpenPreamp.
The plug-in reports zero latency. PAD changes input drive only.

## Build

Requires CMake 3.22+, a C++20 compiler, JUCE (fetched by CMake) and the bundled
GoodLookinUI submodule. The existing macOS build uses Unix Makefiles:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target OpenPreamp_Standalone OpenPreamp_VST3 OpenPreamp_AU -j 6
cmake --build build --config Release --target OpenPreampSmoke -j 6
./build/OpenPreampSmoke_artefacts/Release/OpenPreampSmoke
```

Outputs are in `build/OpenPreamp_artefacts/Release`. On macOS the standalone app
is `Standalone/OpenPreamp.app`. CLAP and LV2 targets are also available.
HybridEQ remains as a separate legacy target; select an **OpenPreamp** target.
The inherited installer scripts still target HybridEQ and should not be used
for packaging OpenPreamp without updating them.

See [CIRCUIT_EXPLAINED.md](CIRCUIT_EXPLAINED.md) for signal-path diagrams and
how the models work. See [OPENPREAMP_HANDOFF.md](OPENPREAMP_HANDOFF.md) for the current implementation
and verification. [HANDOFF.md](HANDOFF.md) is the historical HybridEQ handoff.

## License

Official compiled binaries use [BINARY_LICENSE.txt](BINARY_LICENSE.txt).
Source licensing is in [LICENSE](LICENSE), with dependencies documented in
[THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt). The GoodLookinUI toolkit has
its own MIT license.
