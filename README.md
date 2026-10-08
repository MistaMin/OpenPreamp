# OpenPreamp

Version **0.2.3** (pre-release). Preamp and metering extracted from HybridEQ,
without an EQ, spectrum display or separate harmonics engine.

- **CIRCUIT on:** the selected Brit, N-Type, FSF or A-Type component network
  runs at 2x the session rate. **HQ MODE**, in OUTPUT, raises it to 4x.
- **CIRCUIT off:** the lighter preamp model runs at the session rate with
  first-order antiderivative antialiasing (ADAA). HQ does not change this path.
- **Off / bypass:** no circuit oversampling. Output trim and meters remain active.
- **600 × 840 editor (5:7):** top VU with input/output selection, stereo input
  and output peak ladders, peak hold and resettable clip lamp. Model changes
  load HybridEQ's exact saved plate, knob style and color mappings.
- PAD and GAIN combine into a smoothed drive setting. Double-click a gain knob
  to return to 0 dB. Output trim follows the preamp.

## Oversampling filters and timing

2x uses JUCE's maximum-quality polyphase half-band IIR, with nonlinear phase
and short filter delay. HQ 4x uses two stages of maximum-quality linear-phase
half-band equiripple FIR. First-stage stopband targets are -90 dB up / -75 dB
down; the second FIR stage targets -80 dB up / -65 dB down.

The shorter paths are padded to the rounded-up HQ filter delay, and that fixed
latency is reported during preparation. Switching CIRCUIT/HQ does not change
reported latency while audio is running. Filter histories reset on mode changes;
a transition crossfade is not implemented. ADAA adds its own fractional phase
and high-frequency rolloff as part of the nonlinear processing.

## Build and verify

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target OpenPreamp_VST3 OpenPreampSmoke OpenPreampADAA -j 6
./build/OpenPreampSmoke_artefacts/Release/OpenPreampSmoke
./build/OpenPreampADAA_artefacts/Release/OpenPreampADAA
```

VST3 output: `build/OpenPreamp_artefacts/Release/VST3/OpenPreamp.vst3`.
Standalone/AU/CLAP/LV2 remain available targets; rebuild the requested target
before using artifacts left from older versions. Legacy HybridEQ targets and
installer scripts remain in the checkout. Its installer does not package OpenPreamp.

See [CIRCUIT_EXPLAINED.md](CIRCUIT_EXPLAINED.md) for the circuit diagrams and
[OPENPREAMP_HANDOFF.md](OPENPREAMP_HANDOFF.md) for implementation and verification.
[HANDOFF.md](HANDOFF.md) is the historical HybridEQ handoff.

## License

Compiled binaries use [BINARY_LICENSE.txt](BINARY_LICENSE.txt). Source licensing
is in [LICENSE](LICENSE); dependencies are documented in
[THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt).

## Developer mode

The 0.2.3 editor opens at 600 × 840 and resizes proportionally from 450 × 630
through 1000 × 1400. The VU and output knob are larger, the main gain knob is
slightly smaller, and PAD/MODEL/CIRCUIT/HQ use narrower buttons.

Developer mode is enabled with `-DOPENPREAMP_DEVELOPER_MODE=ON` (default for this
working build). Click **DEV** in the header for a separate window with
**Knobs / layout** and **Model looks** tabs. Knob edits change label, font,
style, colour and geometry immediately. Click either knob to select it.
The model-look tab controls each preamp's plate, faceplate, knob style and colour.
Model looks override the saved knob style/colour when the model is applied.

Edits autosave into `Designs/OpenPreampKnobs.csv` and `Designs/OpenPreampLooks.csv`;
these files are baked into the next build. Set developer mode OFF for a release
without the DEV tools or filesystem autosave. Resizing preserves edited geometry.

The 0.2.3 standalone is available for layout editing. Installed comparison VST3s
remain 0.2.0 and 0.2.2 until a new test installation is requested.
