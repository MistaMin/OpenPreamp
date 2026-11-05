# Plugin QA: VST3: OpenPreamp

| Check | Status | Detail |
|---|---|---|
| loads | PASS | VST3: OpenPreamp (OpenGrid), 28 parameters |
| finite output | PASS | peak -6.5 dBFS |
| not silent | PASS | rms -15.6 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1015x688, 0.0% animates by itself |
| every parameter changes the sound | FAIL | VU Source, Meter Mode, Input / Output Gain Compensation, Meter Type, RMS +3, RMS Weighting |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Preamp Type |  | ok (0.3 dB) | ok (58.320%) |
| 1 | Preamp Pad |  | ok (-3.3 dB) | ok (0.249%) |
| 2 | Preamp Gain |  | ok (3.3 dB) | ok (0.517%) |
| 3 | Preamp Bypass |  | ok (1.0 dB) | ok (3.251%) |
| 4 | Modeled Preamp |  | ok (0.1 dB) | ok (0.249%) |
| 5 | Output Gain |  | ok (-0.6 dB) | ok (0.593%) |
| 6 | Legacy 4x Oversampling | random state 5 | CONDITIONAL (-47.0 dB) | ok (0.324%) |
| 7 | VU Source |  | DEAD (-240.0 dB) | ok (0.419%) |
| 8 | Right / Side Input Gain |  | ok (3.3 dB) | ok (0.502%) |
| 9 | Right / Side Output Gain |  | ok (-0.6 dB) | ok (0.236%) |
| 10 | Mid / Side |  | ok (-14.0 dB) | ok (1.458%) |
| 11 | High Pass |  | ok (-6.4 dB) | ok (0.220%) |
| 12 | Low Pass |  | ok (-6.5 dB) | ok (0.236%) |
| 13 | Input Cuts |  | ok (-11.0 dB) | ok (0.236%) |
| 14 | Link L/R Gains | random state 2 | CONDITIONAL (-3.3 dB) | ok (2.194%) |
| 15 | Mono Maker |  | ok (-24.1 dB) | ok (0.236%) |
| 16 | Mono Maker Frequency | random state 1 | CONDITIONAL (-8.4 dB) | ok (0.236%) |
| 17 | Meter Mode |  | DEAD (-240.0 dB) | ok (0.284%) |
| 18 | Circuit Engine |  | ok (-12.3 dB) | ok (0.296%) |
| 19 | Preamp Oversampling |  | ok (-13.4 dB) | ok (0.212%) |
| 20 | Input / Output Gain Compensation |  | DEAD (-240.0 dB) | ok (1.149%) |
| 21 | Meter Type |  | DEAD (-240.0 dB) | ok (0.579%) |
| 22 | RMS +3 |  | DEAD (-240.0 dB) | ok (0.161%) |
| 23 | RMS Weighting |  | DEAD (-240.0 dB) | ok (0.144%) |
| 24 | Bypass |  | ok (-0.6 dB) | ok (0.144%) |
| 25 | Bypass |  | ok (1.0 dB) | ok (0.628%) |
| 26 | Wet |  | ok (1.0 dB) | ok (0.161%) |
| 27 | Delta |  | ok (-4.8 dB) | ok (0.161%) |
