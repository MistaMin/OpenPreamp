# Hardware and plugin measurement pipeline

`tools/measure_audio.py` generates one hashed PCM fixture, runs the OpenPreamp
offline host, imports calibrated hardware/loopback captures, measures repeated
captures and exports CSV, PNG, SVG and an HTML chart index. It never opens an audio
device or changes interface settings. Hardware measurements require actual
recordings; plugin output and synthetic verification are never hardware evidence.

The default matrix is **Brit, N-Type, FSF, A-Type × Standard, Circuit, HQ ×
1x, 2x, 4x**: 36 explicitly logged configurations per rate. Reports group
charts by preamp; mode uses color and oversampling uses line style. Hardware for
the same model is overlaid when imported. Circuit remains a preview with known
HQ fidelity differences. No pass target is assumed between these engines.
Five overview charts also overlay the four preamps in a grid of every selected
mode and oversampling combination (transfer, distortion, phase, dynamics and aliasing).
The plugin's Default option remains available in the product; measurements use
explicit factors only. Analysis also omits redundant Default rows in old manifests.

## Run plugin comparisons

Use Python 3 with `tools/measurement-requirements.txt` installed in a local virtual
environment. The renderer is a developer console host, not an installed plugin:

```sh
python3 -m venv build/measurement-venv
build/measurement-venv/bin/pip install -r tools/measurement-requirements.txt
cmake --build build --config Release --target OpenPreampSmoke -j 6
build/measurement-venv/bin/python tools/measure_audio.py generate build/measurements/48k-full --rate 48000 --bits 24
build/measurement-venv/bin/python tools/measure_audio.py matrix --suite build/measurements/48k-full --renderer build/OpenPreampSmoke_artefacts/Release/OpenPreampSmoke --out build/measurements/48k-captures --repeats 3 --block 64
build/measurement-venv/bin/python tools/measure_audio.py analyze --suite build/measurements/48k-full --manifests build/measurements/48k-captures/captures.json --out build/measurements/48k-report
```

Open `build/measurements/48k-report/index.html`. Every chart is also an independent
PNG and SVG. `--rate 96000` generates a separate 96 kHz suite. PCM16/24/32 integer
are supported, identically for fixture and capture. Inputs are mono; the plugin
host feeds both channels equally and records channel 1. Export a chosen hardware
return channel as mono PCM without normalization, sample-rate conversion or added
dither. Repeat the workflow for another channel if needed.

`generate --quick` produces a 9.803-second diagnostic at 48 kHz, with fewer levels,
frequencies and shorter settling. It exercises the pipeline; it is not a final
fidelity/long-recovery measurement. The full fixture uses 13 levels, 31 frequency
points, longer settling and a 5-second sweep. At least two independent captures
are required; three is the default. The host resets the processor for each repeat,
retains latency in the WAV and flushes the output tail. All 20 parameters, effective
rate factor, ADAA state, version, offline flag, latency and buffer size are logged.
Input cuts, mono maker, M/S, trims and legacy 4x are explicitly disabled or set to
unity. Internal calculations remain floating point; the capture is quantized to
the same PCM format as the hardware fixture. Clipped samples are counted.

Use `matrix --models Brit --modes Standard Circuit --oversampling 1x 2x` for a
subset. Full matrix captures can be expensive for HQ; progress prints per WAV.
Existing output directories/files are never overwritten. An interrupted run has
`complete: false` and cannot be analyzed as a finished measurement.

## Import hardware and measure the interface baseline first

1. Calibrate and document the interface DAC/ADC full-scale sine levels in dBu,
   gain controls, routing, impedances/loading, cabling and known noise floor.
   Use the same rate, PCM format and input file as the plugin. Match the intended
   hardware operating level (for example, −18 dBFS at the chosen analog reference).
2. Record the fixture directly through the interface loopback at least twice.
   Record before measuring the DUT. Retain its complete pilot, silence windows,
   test sequence and a tail longer than the transport latency. Do not normalize.
3. Insert the hardware without changing converter settings. Record independent
   repeats of the identical fixture. Log every DUT control, threshold/ratio/makeup
   where applicable, and any hardware oversampling/digital mode.
4. Capture sample zero must be synchronous with playback sample zero for both
   loopback and DUT. Known recording offsets may be supplied explicitly as an
   integer `capture_offset_samples` list, one per repeat. Converter clocks must
   be shared. Independent-clock recordings need external drift correction before
   import; this pipeline rejects them rather than mistaking clock drift for THD.

Copy and fill `Docs/measurement-hardware.example.json`; its numeric values are
illustrations, not measured calibration. Use separate settings files for loopback
and DUT, retaining identical `interface`, `calibration` and `capture_origin`.
The `settings` object records all controls; `device` names the hardware. Register
loopback first in a new manifest:

```sh
python3 tools/measure_audio.py register --suite build/measurements/48k-full --manifest build/measurements/hardware.json --id interface-loopback --kind loopback --settings /absolute/path/loopback-settings.json --files /absolute/path/loopback-1.wav /absolute/path/loopback-2.wav /absolute/path/loopback-3.wav
python3 tools/measure_audio.py register --suite build/measurements/48k-full --manifest build/measurements/hardware.json --id brit-hardware --label 'Brit hardware' --kind hardware --model Brit --baseline interface-loopback --settings /absolute/path/brit-settings.json --files /absolute/path/brit-1.wav /absolute/path/brit-2.wav /absolute/path/brit-3.wav
python3 tools/measure_audio.py analyze --suite build/measurements/48k-full --manifests build/measurements/48k-captures/captures.json build/measurements/hardware.json --out build/measurements/comparison
```

The baseline is validated against the same stimulus hash, sample rate, bit depth
and converter settings. Hashes prevent silently analyzing modified files. The
loopback's broadband pilot estimates integer transport latency. That transport
delay is also used for the DUT: the DUT's own pilot arrival does **not** define a
new zero. This preserves DUT delay, polarity and phase. Known recording offsets
are removed separately. Absolute timing accuracy depends on synchronous capture;
the pipeline cannot infer unknown recording-origin jitter separately from DUT
delay. A low pilot correlation or truncated tail is rejected.

Hardware complex sweep response is divided by the loopback complex response;
fundamental output levels are corrected by matched loopback gain at each test
frequency/level. Raw output level and correction remain in CSV. Noise and
distortion are **not** subtracted: the interface floor must be sufficiently below
the DUT to interpret small results. Noise-floor plots include the loopback and
`calibration.known_noise_floor_dbfs_rms` records its prior known floor.

## Definitions and charts

- **Transfer/static curve:** sine peak input vs fundamental peak output in dBFS,
  with the 1:1 line. Gain error is output − input − declared nominal gain. Static
  gain reduction has the opposite sign. Actual gain is preserved. The knee is
  visible only to the resolution of the sampled levels; no inferred threshold is
  invented. Optional `ratio`, `threshold_dbfs`, `makeup_gain_db` are logged and
  included in legends/summary. Register separate captures for each control setting
  to compare several ratios. Preamp models have no compressor ratio control.
- **Spectrum:** 1 kHz at −18 dBFS, Hann window, single-sided peak amplitude per
  FFT bin, H2–H5 marked. Bin width is exported. Capture spectra are averaged as
  power, not as waveforms. This avoids cancelling noise across repeats. Hardware
  bins use the loopback's 1 kHz gain for level calibration, with raw bins retained;
  this does not subtract converter noise or de-embed harmonic-frequency response.
- **THD+N:** fit and remove DC and the known-frequency sine/cosine fundamental.
  Measure residual RMS in the declared unweighted 20 Hz–20 kHz FFT band, using
  Hann window energy normalization. Denominator is fitted **fundamental RMS**
  (`THD+N_F`), not total signal RMS. Average numerator/denominator powers across
  repeats before dividing. Plot versus output at 50 Hz, 1 kHz, 10 kHz and versus
  frequency at −18 dBFS. H2–H5 are separately fitted and exported as dBc; harmonics
  outside the band or at/above Nyquist are unavailable (empty CSV cells), never
  represented as zero distortion. Aliases within band remain in THD+N.
- **Noise:** RMS relative to digital full scale 1, band-limited 20 Hz–20 kHz,
  measured in two silent windows across all repeats. Quantization and DUT tails
  remain. Digital zero uses a numerical display floor of −300 dBFS. The floor is
  not a measurement of analog noise below the converter capability.
- **Aliasing:** eight dedicated coherent sine fixtures: approximately 5, 9, 13
  and 17 kHz, each at −18 and −6 dBFS. Frequencies are snapped to odd FFT bins
  (4,096 measured samples quick; 32,768 full), so known harmonic folds can be
  separated from genuine in-band harmonics. A periodic Hann FFT averages repeat power
  without the extra coherent-tone leakage of a symmetric N−1 window.
  Sum ±2 bins around predicted folds of H2–H31 in 20 Hz–20 kHz, excluding
  fundamental/in-band harmonic collisions; divide by fundamental power and
  express as dBr. Exact frequencies, bin widths, powers and predicted fold
  positions are exported. This finite-order **folded-harmonic proxy** includes
  bin noise/spurs; it is not an isolated or complete measure of all aliasing.
  The input fixture's own quantization/leakage floor appears as a reference.
  Separate plots show the approximately 9 kHz/−6 dBFS spectrum with predicted
  folds marked, and all nonharmonic in-band residual energy (aliases plus noise,
  spurs and transients). Neither metric is THD+N. Hardware traces include ADC
  filtering/aliasing and converter noise; these cannot be attributed solely to
  the analog DUT. Use the loopback evidence before interpreting small differences.
- **Linear sweep harmonic/alias maps:** a separate, continuous 20 Hz → 20 kHz
  linear-frequency chirp at each of −18 and −6 dBFS. Duration is 2 seconds per
  level in quick mode, 8 seconds full, with 20 ms edge fades and a silent guard
  before each sweep. It is distinct from the quiet logarithmic phase sweep.
  Input frequency is the linear horizontal axis, and output frequency extends
  from DC to the session Nyquist frequency (24/48 kHz). Each preamp has a matched
  3×3 Standard/Circuit/HQ × 1x/2x/4x grid, with clean and annotated versions.
  Separate input-reference and imported loopback maps expose analyzer/converter
  artifacts. Predicted H1–H5 trajectories are solid before Nyquist and dashed
  after folding: `abs(((h*f + Fs/2) modulo Fs) − Fs/2)`. Lines predict positions;
  they do not assert that a harmonic is present or uniquely attributable.

  The STFT uses a periodic four-term Blackman–Harris window, with coefficients
  0.35875, 0.48829, 0.14128, 0.01168 and alternating signs. Quick/full FFT sizes
  are 512/1,024 samples, 50% overlap, with no extra zero-padding or time/frequency
  smoothing. Repeats are averaged in squared magnitude. Single-sided FFT bins
  are scaled by coherent window gain, then displayed as dBFS peak per bin.
  DC/Nyquist use their unmirrored scaling. Every panel uses the same −140…0 dBFS
  color limits; raw bins below the display floor are retained. Hardware maps use
  recorded ADC levels without gain normalization, complex de-embedding or noise
  subtraction. Converter calibration and the loopback maps remain essential.

  Captions state rate/PCM width, level, sweep slope/duration, window, FFT/hop,
  bin spacing, equivalent noise bandwidth (ENBW), repeat count, and chirp drift
  per window for H1 and H5. At 48 kHz quick mode, spacing is 93.75 Hz, window
  length 10.67 ms and ENBW about 187.9 Hz. The 9,990 Hz/s sweep moves H1 by
  106.56 Hz and H5 by 532.8 Hz within that window. Bin spacing is **not** the
  effective resolving power of a chirped harmonic. Frequency-dependent DUT delay,
  transients, spectral leakage, fades and overlapping tracks can affect brightness.
  Fades/edge frames are retained with an `interior_frame` flag in raw data.
  These are diagnostic maps, not isolated harmonic-amplitude or THD estimates;
  use the steady-tone results for quantitative distortion. This makes the test
  conditions explicit for the kind of harmonic sweep available in
  [Plugindoctor](https://www.ddmf.eu/plugindoctor/).
- **Dynamics:** continuous 1 kHz tone at −36 → −6 → −36 dBFS, with gain-reduction
  vs time. A centered 4 ms lock-in window smooths the envelope. Events faster than
  that are unresolved; the plot is not an automatic sub-millisecond attack-time
  specification. Transient gain is referred to declared nominal gain. Makeup and
  threshold behavior across settings is also visible in transfer/GR plots.
- **Phase:** −36 dBFS logarithmic sweep with zero tail. Excited FFT bins are
  combined in 180 log-spaced bands; this smoothing is explicit and no phase is
  extrapolated into unexcited bins. Plot magnitude, wrapped/unwrapped phase and
  group delay `−dφ/df / (2π)` in ms. The 1 kHz tone phase is independently logged
  in `summary.json` and `tones.csv`. Plugin latency is removed using the host's
  reported value; hardware uses loopback latency then complex de-embedding.
- **Phase difference:** plugin/hardware shortest signed angular difference on
  the same axes, with optional tolerance band. No polarity flip or best-fit phase
  alignment hides discrepancies. Nonlinear sweep leakage and low SNR can bias
  phase; check the tone phase, noise and capture repeat variation before acceptance.

The scope of THD+N must include level, frequency, gain and bandwidth; see
[Audio Precision's measurement discussion](https://www.audioprecision.com/blog/thd-and-thdn-similar-but-not-the-same).
The phase derivative definition follows
[SciPy's group-delay documentation](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.group_delay.html).
This pipeline declares its fundamental-denominator convention explicitly because
analyzers can use different denominators.

## Targets, raw data and verification

Optional `--targets file.json`, for example:

```json
{"gain_error_db": 0.5, "thdn_percent": 1.0, "phase_difference_deg": 5.0}
```

These numbers are operator-selected examples, not OpenPreamp sound targets.
Plots show gain/phase tolerance bands or THD+N limits. `target-checks.csv` gives
per-tone pass/fail, and `phase-difference.csv` gives per-frequency phase pass/fail.
Without targets the report makes no acceptance claim.

`tones.csv`, `individual.csv`, `spectra.csv`, `phase.csv`, `dynamics.csv`,
`noise.csv` and `noise_windows.csv` preserve analyzed raw series for replotting.
`aliasing.csv`, `alias_spectra.csv` and `alias_markers.csv` retain the alias tests.
`linear-sweep-index.csv` points to per-capture full-resolution `CSV.gz` maps,
including the input reference. Each row gives capture, level, time, input/output
frequency, averaged peak-bin power, dBFS and edge-validity flag. The gzip files
are ordinary CSV after decompression. `linear-sweep-trajectories.csv` exports the
predicted harmonic/fold paths. No map downsampling is applied to raw exports.
The latter retains each silence window/repeat, exposing tails versus initial
noise separately. `configurations.csv` records actual mode/rate/ADAA settings.
`summary.json` stores the
complete plan/settings, capture hashes, noise, clipping, small-signal gain,
1 kHz phase, per-repeat delay/pilot quality and render metadata. Raw WAVs remain
beside capture manifests. Every PNG/SVG contains test conditions and references
the metadata. HTML reports explicitly identify plugin-only runs.

Run verification with:

```sh
python3 -m unittest discover -s tests -p test_measure_audio.py
python3 -m unittest discover -s tests -p test_sweep_spectrogram.py
```

Tests cover integer PCM formats, known harmonic amplitudes/THD+N, bandwidth,
unavailable harmonics, polarity/delay, noise-power averaging, immutable fixtures,
calibration requirements and loopback de-embedding that retains DUT delay, plus
analytic folded-harmonic levels at 48/96 kHz and exclusion of real in-band harmonics.
Sweep-map tests verify chirp cycle count, peak-bin units, power averaging without
phase cancellation, Nyquist coverage, predicted multiple folds, a generated cubic
alias ridge, time/frequency coordinates and full gzip CSV export.
No synthetic test constitutes a physical hardware measurement. Optional SMPTE/
CCIF IMD, live interface capture and automated compressor parameter sweeps are
not included; imported per-ratio fixtures share the existing static/burst analysis.
