# Tube Channel preamp extraction for OpenPreamp

Deferred by the user on 2026-10-16; retained for future work. Excluded from the 1.1 GUI update.

Engineering proposal, 2026-10-16. No tube engine is implemented or released by this document.
The GUI concept uses Peak/RMS. The released OpenPreamp meterMode parameter already
has those exact choices; the VU calibration label remains appropriate on the meter itself.

## Behavioral contract

Add a Tube model derived from Tube Channel's **preamp section**. Retain the discrete
input path, common input buffer, 12AX7 differential stage and its local feedback,
coupling and preamp-output buffer. Remove compressor audio, its 12AT7 makeup stage,
sidechain/detector, opto dynamics, limiter, EQ and post-EQ output chain.
No runtime mains/rectifier/heater/sag simulation or PSU initialization solve.
Use explicit constant supply rails and a solved DC operating point. This is a
fixed-supply preamp derivative, not the full channel strip with processing bypassed.

Recommended initial signal path:

```text
OpenPreamp input tools/drive -> calibrated volts
  -> Tube Channel mic input -> common input buffer/low-cut boundary
  -> 12AX7 differential stage + transistor plate loads/tail + Warm feedback
  -> preamp-output buffer -> defined termination -> calibrated audio
  -> OpenPreamp output trim/meters
```

Start with the microphone path at a fixed, documented reference gain/impedance.
Retain common buffer coloration; place the original low cut at its documented
lowest setting initially and measure its residual response alongside OpenPreamp's
own cuts. Do not silently remove coupling poles to avoid two filters.
Instrument input would be a separately calibrated variant, not an implicit source change.
Keep TUBE20 off and preout fixed at the selected reference position initially.
OpenPreamp INPUT/PAD provide external drive, not a substitute for changing internal feedback.

## What the existing code establishes

ChannelModel already separates mic, inst, tube and preout blocks. Its preamp send
is preout node J6T; the full channel then enters comp/det, four EQ blocks and out.
The bypass switches leave some buffer/coupling coloration and computation in place.
Extract the four blocks rather than keeping the complete channel with bypass flags.
The final balanced output is after EQ, so it is outside the recommended preamp boundary.
If retained later, it must be a deliberate separately measured output-coloration choice.

The tube block contains two triodes, transistor plate-load/current-mirror circuitry,
tail bias, op-amp stages and Warm feedback. A single generic triode waveshaper is
not a model of this entire stage. The compressor's 12AT7 is outside this boundary.

There is a documentation discrepancy: DESIGN.md mentions Q17/Q27 peak-driven bias
reduction, but those devices do not appear in the inspected runtime source/netlists.
Resolve this against source/reference captures before promising that behavior.
Do not infer an implemented mechanism from the design prose alone.

## Replace the supply, preserve its operating conditions

1. Offline, settle the original circuit and record the actual positive/negative
   op-amp rails and tube rail at a fixed reference load, mains setting, plate mode,
   seed and tolerance policy. Record voltages and plate/cathode/grid currents.
2. Freeze those **same** rails in both the original reference and extraction.
   Tube Channel's PSU-off switch freezes present rails, so switching off after
   arbitrary audio history does not define a reproducible reference.
3. Supply the extracted blocks from stored constant rail anchors, preserving the
   original source resistance and rail-dependent op-amp swing. Solve their DC point
   during preparation; initialize capacitor states consistently. No PSU circuit is
   constructed or settled in the shipping preparation path.
4. Do not delete bias, grid-return, tail, plate-load, coupling or local Warm networks.
   Removing the supply animation/simulation does not remove the rails from equations.
5. High/Normal changes both tube rail and the Warm switch network. If both are
   exposed, calibrate each as a distinct configuration and handle transitions.
   Begin with High as a declared reference, not an assumed exact 150 V source.
6. Do not refit sag/ripple into an envelope model for this first derivative. Fixed
   rails deliberately exclude supply-mediated dynamics; retain tube-local memory.

The original stages use one-way Thevenin-driven boundaries. Preserve those for the
first numerical comparison. Audit load resistances, coupling caps, DC returns and
source impedances at V1OUT and J6T. comp.net contains C93 into CRIN, a 100k return,
buffer and rail clamps; this is not automatically an ideal open-circuit load.
Any termination matching or boundary correction must be measured separately from
simple extraction; the original block model does not propagate every downstream
load back upstream. Do not claim it does or fit to a different loading condition.

## HQ first, then reduced modes

- **HQ:** isolated full retained circuit under fixed rails. Reuse the verified Tube
  Channel triode-capable core behind an adapter initially. OpenPreamp's existing
  circuit solver is not a drop-in triode implementation. Preserve MIT attribution
  and source revision if code is copied. Build/parse/DC off the audio thread;
  processing must have bounded work, no heap activity and a declared failure policy.
- **Circuit:** fit against this extracted HQ, not the original whole-channel output
  or its Eco engine. Start with measured linear filters and asymmetric nonlinear
  maps; add state where bursts/history tests show tube-local bias or capacitor memory.
  A static curve passing steady tones is insufficient evidence for transient fidelity.
- **Standard:** a deliberately lighter Tube voicing, with separately documented
  approximation limits. Do not label an unrelated saturation preset a matched model.
- **Antialiasing:** retain 1x/2x/4x. Standard/Circuit at 1x use validated ADAA on suitable
  nonlinear residuals; turn it off at 2x/4x. This is not automatic ADAA of the entire
  stateful circuit. HQ retains the circuit path without added ADAA. Match all rate
  comparisons to an oversampled offline reference and report latency/phase effects.

Follow the HQ-to-Eco guide: freeze reference, profile, declare CPU/absolute callback
budgets, capture training data, fit, validate on independent inputs, then integrate.
Existing Tube Channel Eco coefficients cannot be assumed valid after changing rails,
termination, boundaries or calibration. Reuse tooling, validate/re-fit the data.

## Validation and integration sequence

1. Pin source and netlists; build a reference harness that taps preout J6T under
   explicit fixed rails, with noise/tolerances off. Declare volts/dBFS mapping,
   load, gain, plate mode, all controls and warmup before capturing anything.
2. Build extraction and compare stage taps, DC voltages/currents, gain and frequency/
   phase response, H2-H5/THD+N, asymmetric overload, silence and recovery. Compare
   extraction at identical rates before evaluating any reduction.
3. Capture slow transfer sweeps, multiple amplitudes/frequencies, linear harmonic
   maps, multitone/IMD, DC-offset sensitivity, bursts and overload recovery. Use
   long-enough settling for the actual capacitor time constants. Keep training and
   validation stimuli separate, including program material for listening checks.
4. Record pass/fail limits before fitting; numerical extraction agreement and reduced
   perceptual tolerances are different gates. Report failed gates and do not promote
   a model based only on a version bump, speed or a steady-tone match.
5. Append Tube to the model identity space; do not reorder Brit/N-Type/FSF/Off/A-Type.
   Keep Off first only in presentation and N-Type initial. Test old project restore
   and host automation mappings, especially choice range changes. New parameters,
   if needed for plate mode, go after existing ones with explicit old-state defaults.
6. Test mono, stereo, M/S, independent channel histories, linked trims, compensation,
   wheel/keyboard gestures, bypass, all rates, variable buffer partitioning, resets,
   sample-rate changes, denormals and nonfinite/solver failure handling. Benchmark
   CPU separately with other workloads stopped; verify callback allocations.
7. Extend the report to **5 models x 3 modes x 3 rates = 45 configurations**, with
   two captures each (90), only after each path exists. If both plate modes are
   exposed, the full matrix is 36 existing configurations plus 18 Tube cases = 54
   (108 captures), not 90 configurations. Archive settings, hashes and raw CSV.

No hardware-equivalence or listening approval is established by this proposal.
The first deliverable is a fixed-supply preamp extraction and reference comparison,
followed by calibrated Circuit/Standard paths and product integration.

## Source inspected

Tube Channel commit: `c55d47e8ca636122a400383914aab459ac20636b`. The checkout also contains unrelated documentation
edits; no Tube Channel files were changed during this assessment. File hashes pin
exact inspected circuit inputs and code.

| File | SHA-256 |
|---|---|
| `netlists/mic.net` | `ffff3777c3e07e246f89ef01d11f16b698cbee571acb0fa70b664ef261c8f31d` |
| `netlists/inst.net` | `4b65af146e1cd486f90db7b244103769a5ac15cd5f19bb2eaee942366b92961c` |
| `netlists/tube.net` | `07e88695925f94eee9c5a25df8d718b637ea3925e0ca93baf87b3044df967195` |
| `netlists/preout.net` | `585f7d75e44b70b6997968270b8c577210b76b868d604ea6088c24e82cd0944c` |
| `netlists/comp.net` | `bfcd2f56c6242190e4ee2344485abf0c5251e34289988cd18769eb90687070de` |
| `Source/Channel/ChannelModel.h` | `a7ba475bf7472f80ff0b5ddaf95d96becf8953349df88b9c543fd551f9950393` |
| `Source/Circuit/Block.h` | `9abcfe22eb19d040d644ad97a8cdd8261b49433ae7fc5b0625cbcc5a0617fac1` |
| `Source/Circuit/Circuit.h` | `e52b2fc6247002351738c1046a164352d012d5e3691fb5523ccf465ac1741655` |

Method reference: Macak & Schimmel, DAFx 2010,
[Real-Time Guitar Tube Amplifier Simulation using an Approximation of Differential Equations](https://www.dafx.de/paper-archive/2010/DAFx10/MacakSchimmel_DAFx10_P12.pdf).
It explicitly uses a constant preamp supply and state-dependent approximation;
this supports the method, not equivalence of this particular extracted circuit.
