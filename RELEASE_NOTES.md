# OpenPreamp 1.0.0

A preamp and metering tool from OpenGrid. Choose a lighter antialiased model or
solve a component network at 2x / HQ 4x, then shape stereo or Mid/Side drive with
independent trims, shared cuts and a Side-only mono maker.

## Included

- Universal Mac VST3, Audio Unit, CLAP and LV2; macOS 11 or later.
- Production interface with developer tools disabled, resizing down to 400 x 296.
- Illustrated PDF manual covering every control, signal flow and circuit modeling.
- MIT project/source license, GoodLookinUI license and complete third-party notices.
- AAX binaries are intentionally excluded from downloads.

## Processing and metering

Brit, N-Type, FSF and A-Type plus a clean Off model; independent L/R or M/S
input/output trims; Link; -20 dB / Unity / +10 dB PAD; 12 dB/octave high/low cuts;
6 dB/octave Side-only mono maker; independent VUs with direct-click Input/Output
and Peak/RMS controls, peak bars, peak hold and resettable clip lamps.

Circuit off uses the lighter session-rate model with ADAA. Circuit on uses the
full component network at 2x with IIR resampling. HQ uses the same circuit at
4x with FIR resampling. Surrounding linear tools remain at session rate.

## Practical limits

Mono and stereo only; no mono-to-stereo or surround layout. HQ's linear phase
applies to its resampling filters, not the complete circuit/cuts signal path.
Circuit/oversampling mode changes reset histories without a transition crossfade.
Models contain estimated transformer parameters and simplified device models;
they are not measurements or certified reproductions of individual hardware.
Meters report sample peaks, not intersample true peaks. There is no limiter.

AAX builds remain private and require their own Avid/PACE workflow before they
could be used or distributed. Neither the AAX binary nor SDK is included here.
