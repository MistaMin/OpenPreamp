# OpenPreamp metering (unreleased)

MODE selects VU, RMS, K-12, K-14, K-20, DIN, Nordic, BBC, EBU, or sample Peak.
Both mode selectors share the same setting. SOURCE selects the input or output tap
for both independent Left/Right meters, or Mid/Side meters in M/S mode.
Measurements run at the session sample rate, before/after the existing audio path;
closing the editor or changing its refresh rate does not change detector timing.

VU uses a two-stage average-rectified detector, calibrated so a steady 1 kHz sine
with a peak of -18 dBFS reads 0 VU. Its rise reaches 99% after approximately 300 ms.
The needle uses an amplitude scale; this is separate from sample-peak detection.

RMS uses 300 ms exponential mean-square averaging. Its one-decimal numeric readout
uses a 0.02 dB margin at rounding boundaries to prevent last-digit flicker; the
underlying measurement and needle retain the detector's full precision. The plain RMS default is raw:
a full-scale sine reads -3.0103 dBFS. RMS +3 adds 3.0103 dB, so that sine reads
0 dBFS. Changing RMS +3 preserves the detector history and shifts the scale
without a meter drop or refill. K scales always use sine-normalized RMS, with zero at -12, -14 or -20 dBFS.
K scales and K frequency weighting are independent settings. These are RMS meters,
not gated LUFS or integrated loudness meters.

WEIGHT selects None (flat), A, B, C, D or K for RMS and K scales. The other modes
ignore weighting. A/B/C/D use bilinear-transformed analog prototypes normalized
at 1 kHz. K uses the BS.1770 shelf and high-pass cascade, adapted to the session
sample rate, without the loudness algorithm's -0.691 offset. Bilinear frequency
warping increases near Nyquist; this is not a certified IEC sound-level meter.

PPM modes use full-wave quasi-peak envelopes. DIN and Nordic have approximately
5 ms integration; BBC and EBU approximately 10 ms (a 1 kHz burst reads about 2 dB
below a sustained tone). DIN falls 20 dB in 1.5 s, Nordic 20 dB in 1.7 s,
BBC 26 dB in 3 s and EBU 24 dB in 2.8 s. These are software models checked with
1 kHz bursts and fall-rate fixtures, not an exhaustive IEC conformance claim.
Alignment at -18 dBFS is DIN/EBU 0, Nordic +6, and BBC 4. Cream, black and PPM
needle faces show the corresponding scale with the original skin's bold lettering,
curve and tick weights. The large decorative initials are replaced with the meter
type and reference level. Every visual face also shows its source, meter type and
reference in the header; all 16 face styles remain available.

The IN/OUT peak bars, peak hold and CLIP lamp always use unweighted sample peaks.
RMS +3 and weighting affect the average measurement only. CLIP is latched and
can be reset by clicking its lamp. No metering option changes the audio samples.

Old sessions restore MODE to Legacy, which follows the original Peak/RMS setting.
The LEGACY Peak/RMS keys are enabled in this mode. The original parameter keeps
its original two choices and normalized automation values. New meter controls are
appended to the parameter list, and all meter settings save with the host state.

## Engineering references

- [ITU-R BS.645-2](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.645-2-199203-I%21%21PDF-E.pdf): VU/PPM timing definitions.
- [EBU Tech 3205](https://tech.ebu.ch/files/live/sites/tech/files/shared/tech/tech3205.pdf): Type IIb metering.
- [MathWorks weighting-filter reference](https://www.mathworks.com/help/audio/ref/weightingfilter.html): analog prototypes, bilinear transform and K coefficients.
- [Micro-Cap reference manual](https://www.iee.et.tu-dresden.de/~jmueller/simulation/soft/microcap/MC12.RefManual.pdf): A/B/C/D transfer functions, FWeighting section.
