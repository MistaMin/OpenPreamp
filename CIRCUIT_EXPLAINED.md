# OpenPreamp circuit — version 0.3.1

OpenPreamp simulates a preamp; it does not control physical hardware.
Only the selected preamp circuit is oversampled. Its surrounding processors
operate at the DAW session rate.

## Plug-in signal path

```mermaid
flowchart LR
    I[DAW L/R input] --> E{M/S mode}
    E -->|On| MS[Session-rate M/S encoder]
    E -->|Off| LR[Independent L/R streams]
    MS --> HP[Shared 12 dB/oct high pass]
    LR --> HP
    HP --> LP[Shared 12 dB/oct low pass]
    LP --> G[Independent input trims + shared PAD]
    G --> M{CIRCUIT}
    M -->|On| U[2x IIR / HQ 4x FIR upsample]
    U --> C[Independent preamp circuits]
    C --> D[Downsample to session rate]
    M -->|Off| A[Independent light preamps + ADAA at session rate]
    D --> P[Fixed latency padding]
    A --> P
    P --> T[Independent output trims at session rate]
    T --> DEC{M/S mode}
    DEC -->|On| DE[Session-rate M/S decoder]
    DEC -->|Off| O[DAW L/R output]
    DE --> O
    MS -.-> V[Channel input VUs]
    LR -.-> V
    T -.-> OV[Channel output VUs]
```

Input trims and PAD are applied before upsampling, with 10 ms linear gain
ramps at the session rate. The internal circuit gain control is held at unity.
This is intentionally different from 0.2.3, where GAIN changed the circuit's
feedback/gain network: 0.3.0 uses input drive into a fixed-gain circuit.
The lighter preamp path also receives its drive externally and retains ADAA.

The cuts are two-pole Butterworth IIR filters (12 dB/octave), shared frequency
settings with independent L/R or M/S state. They default to high-pass 20 Hz
and low-pass 20 kHz, have smoothed frequency changes and clamp below Nyquist.
They run entirely at the session rate. CUTS bypasses both filters. The preamp
bypass also skips input trims/PAD and cuts, leaving output trims and metering.
Model Off retains trims/cuts but skips the colored preamp circuit.

M/S uses an orthonormal matrix: M = (L + R) / sqrt(2), S = (L - R) / sqrt(2).
The inverse runs after downsampling, latency padding and M/S output trims:
L = (M + S) / sqrt(2), R = (M - S) / sqrt(2). Equal settings with a clean path
reconstruct stereo unchanged apart from the reported delay. The right strip
controls Side in M/S mode. Mono hosts keep the left stream and ignore M/S.

Each preamp owns its own solver and ADAA history. Circuit on selects 2x;
HQ selects 4x. Existing oversampling filters and fixed reported latency are
retained. Filters and gain trims are outside this multirate path.

The two VUs independently measure input/output RMS, peak hold and clip.
In L/R mode they show Left and Right; in M/S mode they show Mid and Side.
Input taps are after encoding and before cuts/drive. Output taps are after
channel output trims and before decoding. They read audio without changing it.
The editor reads the shared level accumulator once per tick and distributes
that reading to both meters, including the hidden second strip. 0 VU = -18 dBFS.
The clip lamp is an indicator, not a limiter.

## Default N-Type circuit

```mermaid
flowchart LR
    I[Input voltage] --> IT[Input transformer]
    IT --> CC[Input coupling capacitor]
    CC --> G[Three-transistor gain stage]
    G --> OC[Interstage coupling]
    OC --> D[Class-A output driver]
    D --> OT[Output transformer]
    OT --> L[600-ohm load + Zobel]
    L --> N[Digital level normalization]
    G -. negative feedback .-> G
    D -. local feedback .-> D
```

These blocks explain the topology. The solver solves the interconnected network
together rather than processing six unrelated effects in sequence.

1. **Input transformer:** the modeled VTB9046 has a 4:1 primary-to-secondary
   ratio. It steps voltage down. Winding resistance, leakage inductance,
   magnetizing inductance and winding capacitance contribute to the response.
   The magnetizing inductance falls as its modeled core saturates, so low-end
   behavior depends on level. It is not a recorded impulse response.
2. **Coupling capacitor:** separates the signal from the amplifier's DC bias.
   Its interaction with surrounding impedances affects bass response and phase.
3. **Three-transistor gain stage:** BC184C-family transistor models amplify
   the signal. A feedback network returns part of the output to control gain.
   The gain control changes its R11 resistor, altering feedback instead of
   merely multiplying the finished output. Junction nonlinearity and headroom
   make the response depend on signal level and gain.
4. **Interstage coupling:** passes the changing audio signal to the driver
   while maintaining the stages' separate operating points.
5. **Class-A output driver:** modeled small-signal transistors drive a 2N3055
   power transistor. Its DC bias establishes an operating point; the transistor
   and output-transformer network provide the loaded output. Supply limits,
   bias and nonlinear device behavior determine overload behavior.
6. **Output transformer and load:** the VTB9049 model has a 1:1.7 ratio and a
   gapped saturating core. The simulated 600-ohm load affects the actual network
   solution. The Zobel is a resistor/capacitor damping network that loads the
   high-frequency response. Estimated core parameters affect low-frequency
   saturation; there is no magnetic hysteresis model.
7. **Normalization:** small-signal gain is measured at 1 kHz during preparation
   and used to normalize the reference output level. It does not remove the
   circuit's frequency response or distortion, and is not automatic loudness
   matching as you turn up drive.

## Other models

| Model | Circuit topology | What the sections do |
|---|---|---|
| Brit | Balanced input/RF filters → matched transistor pair with current feedback → instrumentation pair → difference amplifier → DC servos → balanced driver | Input filtering reduces RF; the balanced stages amplify the difference between the two legs; the gain pot changes two stages together; DC servos control offset; the driver supplies the differential output. |
| A-Type | RF input network → 1:8 input transformer → op-amp with T feedback network → coupling network → 1:2 output transformer → pad/load | Input transformer steps voltage up; pot and switched feedback leg set amplifier gain; coupling separates bias; output transformer and load shape the delivered output. The discrete op-amp and transformer parameters include placeholders. |
| FSF | 1:5 input transformer → attenuator ladder/12-position gain switch → op-amp → coupling → op-amp/class-AB transistor pair → output transformer/load | Hardware gain is stepped; software selects a nearby switch position and supplies trim. The class-AB pair supplies output current. A tertiary transformer winding feeds the output behavior back into the driver loop. Output transformer parameters include placeholders. |

## How the simulation operates

Preparation constructs the network and finds its DC operating point. For each
audio sample, digital amplitude is converted to the model's calibrated input
voltage. The solver enforces current balance at the circuit nodes and solves
for node voltages and inductor/transformer branch currents.

Resistors have a linear voltage/current relationship. Capacitors retain voltage
and current history; inductors retain flux and voltage history. Nonlinear
transistor junctions and saturating inductors make the equations depend on the
unknown solution. The solver uses Newton iterations, starting from the previous
sample, until the network is consistent. Normally it permits up to eight
iterations; a failed step triggers smaller input increments and additional
iterations. The output-node voltage is converted back to digital amplitude.
Stereo has separate circuit state for left and right.

That repeated network solve explains the CPU cost. A higher session rate means
more solves per second. OpenPreamp 0.2.3 uses 2x IIR or HQ 4x FIR resampling for the circuit path,
without decimating the circuit back to a fixed rate. Host latency stays fixed
by padding the shorter paths. The lighter models use first-order ADAA: each
nonlinearity averages its response between consecutive sample values using
an antiderivative (or numerical integration). This reduces aliases at standard
session rates and adds fractional phase/high-frequency rolloff. The full
stateful circuit solver is not changed by ADAA.

The method follows [antiderivative antialiasing for memoryless nonlinearities](https://www.research.ed.ac.uk/files/34115216/bilbao_pdf.pdf).

This is a component-network simulation with estimated parameters and simplified
device models. Brit/FSF use behavioral op-amp models; A-Type's discrete op-amp
is a placeholder. Estimated transformer behavior is not a measured reproduction
of a specific physical unit.

Implementation: Source/OpenPreampPlugin/PluginProcessor.cpp,
Source/DSP/Preamp.h, Source/DSP/CircuitSolver.h and the four *Circuit.h files.
