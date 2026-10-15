#!/usr/bin/env python3
"""Repeatable hardware/plugin measurement, PCM fixtures, CSV evidence and charts.

No live audio devices are opened. Hardware captures are imported explicitly.
See Docs/MEASUREMENT_PIPELINE.md for definitions and calibration requirements.
"""
from __future__ import annotations
import argparse
import csv
import datetime as dt
import hashlib
import json
import math
import sys
from pathlib import Path
import subprocess
import wave
import numpy as np

MODELS = {"Brit": 0, "N-Type": 1, "FSF": 2, "A-Type": 4}
MODES = ("Standard", "Circuit", "HQ")
RATES = ("Default", "1x", "2x", "4x")
SCHEMA = 1


def stamp():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def save_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2, allow_nan=False) + "\n")


def load(path):
    return json.loads(Path(path).read_text())


def db(x):
    return 20 * np.log10(np.maximum(np.abs(x), 1e-15))


def write_wav(path, samples, rate, bits=24):
    # Fixed quantization, no random dither: captured noise remains measurable.
    scale = 2 ** (bits - 1)
    data = np.rint(np.clip(samples, -1, 1 - 1 / scale) * scale).astype(np.int64)
    packed = np.stack([(data >> (8 * i)) & 255 for i in range(bits // 8)], axis=1).astype("u1")
    with wave.open(str(path), "wb") as w:
        w.setparams((1, bits // 8, rate, len(data), "NONE", "not compressed"))
        w.writeframes(packed.tobytes())


def read_wav(path):
    with wave.open(str(path), "rb") as w:
        if w.getnchannels() != 1 or w.getsampwidth() not in (2, 3, 4):
            raise ValueError(f"{path}: mono 16/24/32-bit integer PCM required")
        rate, bits = w.getframerate(), 8 * w.getsampwidth()
        raw = np.frombuffer(w.readframes(w.getnframes()), dtype="u1").reshape(-1, bits // 8)
    unsigned = sum(raw[:, i].astype(np.int64) << (8 * i) for i in range(bits // 8))
    signed = (unsigned ^ (1 << (bits - 1))) - (1 << (bits - 1))
    return signed.astype(float) / (1 << (bits - 1)), rate, bits


def export_csv(path, rows):
    rows = list(rows)
    if not rows:
        return
    keys = list(dict.fromkeys(k for row in rows for k in row))
    with Path(path).open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=keys)
        writer.writeheader()
        for row in rows:
            writer.writerow({k: ("" if isinstance(v, (float, np.floating)) and not np.isfinite(v) else v)
                             for k, v in row.items()})


def generate(out, rate=48000, bits=24, quick=False):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=False)
    pieces, segments, position = [], [], 0

    def append(kind, samples, **metadata):
        nonlocal position
        samples = np.asarray(samples, dtype=float)
        segments.append(dict(kind=kind, start=position, stop=position + len(samples), **metadata))
        pieces.append(samples)
        position += len(samples)

    def silence(seconds, kind="guard"):
        append(kind, np.zeros(round(rate * seconds)))

    silence(.05)
    pilot = np.random.default_rng(1701).normal(0, .08, round(.06 * rate))
    pilot *= np.hanning(len(pilot))
    append("pilot", pilot)
    silence(.2 if quick else 1)
    silence(.12 if quick else 2, "noise")
    duration = .6 if quick else 5
    t = np.arange(round(duration * rate)) / rate
    k = np.log(20000 / 20) / duration
    sweep = 10 ** (-36 / 20) * np.sin(2 * np.pi * 20 / k * np.expm1(k * t))
    fade = min(round(.01 * rate), len(sweep) // 8)
    sweep[:fade] *= np.sin(np.linspace(0, np.pi / 2, fade)) ** 2
    sweep[-fade:] *= np.cos(np.linspace(0, np.pi / 2, fade)) ** 2
    tail = round((.2 if quick else 1) * rate)
    append("sweep", np.r_[sweep, np.zeros(tail)], level_dbfs=-36, duration_s=duration,
           frequency_start_hz=20, frequency_stop_hz=20000)
    silence(.05)

    def tone(frequency, level, purpose):
        settle = max(.04 if quick else .25, 2 / frequency)
        measure = max(.06 if quick else 1, 6 / frequency)
        # Integer cycles in the analyzed portion; exact 1 kHz reference retained.
        n = round(measure * frequency) * rate / frequency
        n = round(n)
        warm = round(settle * rate)
        t = np.arange(warm + n) / rate
        append("tone", 10 ** (level / 20) * np.sin(2 * np.pi * frequency * t),
               frequency_hz=frequency, level_dbfs=level, purpose=purpose,
               analyze_start=position + warm, analyze_stop=position + warm + n)

    levels = [-48, -30, -18, -6] if quick else [-60, -54, -48, -42, -36, -30, -24, -18, -12, -9, -6, -3, -1]
    for frequency in [50, 1000, 10000]:
        for level in levels:
            tone(frequency, level, "level")
    frequencies = [50, 100, 1000, 5000, 10000, 18000] if quick else np.geomspace(20, 20000, 31)
    for frequency in frequencies:
        tone(float(frequency), -18, "frequency")
    silence(.1)
    durations = [.15, .2, .25] if quick else [.5, 1, 2]
    counts = [round(d * rate) for d in durations]
    envelope = np.r_[np.full(counts[0], 10 ** (-36 / 20)),
                     np.full(counts[1], 10 ** (-6 / 20)),
                     np.full(counts[2], 10 ** (-36 / 20))]
    t = np.arange(len(envelope)) / rate
    append("dynamics", envelope * np.sin(2 * np.pi * 1000 * t), frequency_hz=1000,
           low_dbfs=-36, high_dbfs=-6, attack_sample=counts[0],
           release_sample=counts[0] + counts[1], envelope_window_ms=4)
    silence(.2 if quick else 2)
    silence(.12 if quick else 2, "noise")
    samples = np.concatenate(pieces)
    write_wav(out / "stimulus.wav", samples, rate, bits)
    plan = dict(schema=SCHEMA, created_utc=stamp(), sample_rate=rate, bit_depth=bits,
                quick=quick, analysis_band_hz=[20, 20000], tone_level_convention="sine peak dBFS; full-scale sine = 0 dBFS",
                noise_level_convention="RMS relative to 1 digital full scale; unweighted 20 Hz–20 kHz",
                thdn_denominator="fundamental RMS (THD+N_F)", stimulus_sha256=sha(out / "stimulus.wav"),
                frames=len(samples), segments=segments)
    save_json(out / "plan.json", plan)
    export_csv(out / "segments.csv", segments)
    print(f"Generated {len(samples)/rate:.3f}s, {rate} Hz, PCM{bits}: {out}")
    return plan


def suite(path):
    root = Path(path).resolve()
    plan = load(root / "plan.json")
    if plan["schema"] != SCHEMA or sha(root / "stimulus.wav") != plan["stimulus_sha256"]:
        raise ValueError("unsupported or modified stimulus suite")
    x, rate, bits = read_wav(root / "stimulus.wav")
    if (rate, bits, len(x)) != (plan["sample_rate"], plan["bit_depth"], plan["frames"]):
        raise ValueError("stimulus format differs from plan")
    return root, plan, x


def capture_matrix(args):
    root, plan, _ = suite(args.suite)
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=False)
    exe = Path(args.renderer).resolve()
    records, jobs = [], []
    for model in args.models:
        for mode in args.modes:
            for os in args.oversampling:
                ident = f"{model}-{mode}-{os}"
                params = dict(preampType=MODELS[model], preampCircuit=int(mode != "Standard"),
                              circuitEngine=int(mode == "Circuit"), preampOversampling=RATES.index(os),
                              hqMode=0, cutsEnabled=0, monoMakerEnabled=0, midSide=0,
                              preampPad=1, preampGain=0, preampGainR=0, outputGain=0, outputGainR=0)
                files = []
                for repeat in range(args.repeats):
                    path = out / f"{ident}-{repeat+1}.wav"
                    files.append(str(path))
                    jobs.append(dict(input=str(root / "stimulus.wav"), output=str(path),
                                     block_size=args.block, bit_depth=plan["bit_depth"], parameters=params))
                records.append(dict(id=ident, label=f"{mode} {os}", model=model, mode=mode,
                                    oversampling=os, kind="plugin", created_utc=stamp(),
                                    files=files, settings=dict(nominal_gain_db=0, parameters=params),
                                    renderer_sha256=sha(exe)))
    save_json(out / "jobs.json", jobs)
    # Save an explicitly incomplete manifest so an interrupted capture cannot look complete.
    manifest = dict(schema=SCHEMA, stimulus_sha256=plan["stimulus_sha256"], complete=False, captures=records)
    save_json(out / "captures.json", manifest)
    subprocess.run([str(exe), "--measure-batch", str(out / "jobs.json")], check=True)
    for record in records:
        record["file_sha256"] = [sha(p) for p in record["files"]]
        record["render_metadata"] = [load(p + ".json") for p in record["files"]]
    manifest["complete"] = True
    save_json(out / "captures.json", manifest)
    print(f"Captured {len(records)} configurations × {args.repeats} repeats")


def validate_hardware(settings):
    for field in ("device", "settings", "interface", "calibration", "capture_origin", "nominal_gain_db"):
        if field not in settings:
            raise ValueError(f"hardware settings missing {field}")
    if settings["capture_origin"] != "playback-synchronous":
        raise ValueError("hardware requires playback-synchronous capture origin; trim only a known recording offset")
    for field in ("date", "dac_full_scale_dbu", "adc_full_scale_dbu", "known_noise_floor_dbfs_rms", "clock"):
        if field not in settings["calibration"]:
            raise ValueError(f"calibration missing {field}")
    if settings["calibration"]["clock"] != "shared":
        raise ValueError("independent converter clocks require drift correction before import")
    for field in ("dac_full_scale_dbu", "adc_full_scale_dbu", "known_noise_floor_dbfs_rms"):
        if not isinstance(settings["calibration"][field], (int, float)) or not math.isfinite(settings["calibration"][field]):
            raise ValueError(f"calibration {field} must be a finite number")
    dt.date.fromisoformat(settings["calibration"]["date"])
    if not math.isfinite(float(settings["nominal_gain_db"])):
        raise ValueError("nominal_gain_db must be finite")


def register(args):
    _, plan, _ = suite(args.suite)
    path = Path(args.manifest)
    manifest = load(path) if path.exists() else dict(schema=SCHEMA, stimulus_sha256=plan["stimulus_sha256"], complete=True, captures=[])
    if manifest["stimulus_sha256"] != plan["stimulus_sha256"] or not manifest["complete"]:
        raise ValueError("manifest incomplete or uses another stimulus")
    if any(c["id"] == args.id for c in manifest["captures"]):
        raise ValueError("capture id already exists")
    settings = load(args.settings)
    validate_hardware(settings)
    files = [str(Path(f).resolve()) for f in args.files]
    if len(files) < 2 or len(set(files)) != len(files):
        raise ValueError("at least two independent captures required")
    for f in files:
        y, rate, bits = read_wav(f)
        if rate != plan["sample_rate"] or bits != plan["bit_depth"] or len(y) < plan["frames"]:
            raise ValueError(f"capture format/length mismatch: {f}")
    if args.kind == "hardware":
        baseline = next((c for c in manifest["captures"] if c["id"] == args.baseline and c["kind"] == "loopback"), None)
        if not baseline:
            raise ValueError("register the loopback baseline before hardware")
        for key in ("interface", "calibration", "capture_origin"):
            if baseline["settings"][key] != settings[key]:
                raise ValueError(f"hardware/loopback {key} differs")
    record = dict(id=args.id, label=args.label or args.id, kind=args.kind, model=args.model,
                  mode="Hardware" if args.kind == "hardware" else "Loopback", oversampling="n/a",
                  files=files, file_sha256=[sha(f) for f in files], settings=settings,
                  baseline=args.baseline, created_utc=stamp())
    manifest["captures"].append(record)
    path.parent.mkdir(parents=True, exist_ok=True)
    save_json(path, manifest)


def pilot_delay(x, y, plan):
    segment = next(s for s in plan["segments"] if s["kind"] == "pilot")
    stop = segment["stop"]
    ref = x[:stop]
    search = y[:min(len(y), stop + plan["sample_rate"])]
    n = 1 << (len(search) + len(ref) - 1).bit_length()
    correlation = np.fft.irfft(np.fft.rfft(search, n) * np.conj(np.fft.rfft(ref, n)), n)
    index = int(np.argmax(np.abs(correlation[:len(search) - len(ref) + 1])))
    energy = np.linalg.norm(ref) * np.linalg.norm(search[index:index + len(ref)])
    score = abs(correlation[index]) / max(energy, 1e-30)
    if score < .5:
        raise ValueError(f"alignment pilot correlation too low ({score:.3f})")
    return index, score


def band_power(samples, rate, band=(20, 20000)):
    samples = np.asarray(samples)
    w = np.hanning(len(samples))
    spectrum = np.fft.rfft(samples * w)
    frequencies = np.fft.rfftfreq(len(samples), 1 / rate)
    weights = np.full(len(spectrum), 2.)
    weights[0] = 1
    if len(samples) % 2 == 0:
        weights[-1] = 1
    mask = (frequencies >= band[0]) & (frequencies <= band[1])
    return float(np.sum(weights[mask] * np.abs(spectrum[mask]) ** 2) / (len(samples) * np.sum(w ** 2)))


def tone_metrics(samples, rate, frequency, band=(20, 20000)):
    t = np.arange(len(samples)) / rate
    # Fit DC + exact fundamental, retaining its phase and amplitude.
    basis = np.column_stack([np.sin(2 * np.pi * frequency * t), np.cos(2 * np.pi * frequency * t), np.ones(len(t))])
    coefficients = np.linalg.lstsq(basis, samples, rcond=None)[0]
    fundamental_power = float(np.sum(coefficients[:2] ** 2) / 2)
    residual = samples - basis @ coefficients
    residual_power = band_power(residual, rate, band)
    harmonics = []
    for order in range(2, 6):
        f = frequency * order
        if f > min(band[1], rate / 2) or f >= rate / 2:
            harmonics.append(float("nan"))
            continue
        harmonic_basis = np.column_stack([np.sin(2 * np.pi * f * t), np.cos(2 * np.pi * f * t), np.ones(len(t))])
        c = np.linalg.lstsq(harmonic_basis, residual, rcond=None)[0]
        harmonics.append(float(np.sum(c[:2] ** 2) / 2))
    return dict(fundamental_power=fundamental_power, residual_power=residual_power,
                phase_rad=float(np.angle(coefficients[0] + 1j * coefficients[1])), harmonics=harmonics)


def sweep_transfer(x, y, rate):
    n = 1 << (len(x) - 1).bit_length()
    X, Y = np.fft.rfft(x, n), np.fft.rfft(y, n)
    f = np.fft.rfftfreq(n, 1 / rate)
    # Complex log-bin averaging without magnitude-only normalization. Report
    # only bins with adequately excited source energy; no extrapolation.
    centers = np.geomspace(25, 19000, 180)
    values = []
    for center in centers:
        mask = (f >= center / 1.035) & (f <= center * 1.035) & (np.abs(X) > np.max(np.abs(X)) * 1e-4)
        if not np.any(mask):
            values.append(complex(np.nan, np.nan))
        else:
            values.append(np.sum(Y[mask] * np.conj(X[mask])) / np.sum(np.abs(X[mask]) ** 2))
    return centers, np.asarray(values)


def envelope_trace(y, x, rate, window_ms=4):
    n = max(1, round(rate * window_ms / 1000))
    oscillator = np.exp(-2j * np.pi * 1000 * np.arange(len(y)) / rate)
    kernel = np.ones(n) / n
    numerator = 2 * np.abs(np.convolve(y * oscillator, kernel, "same"))
    denominator = 2 * np.abs(np.convolve(x * oscillator, kernel, "same"))
    gain = db(numerator / np.maximum(denominator, 1e-15))
    gain[:n] = np.nan
    gain[-n:] = np.nan
    return gain


def read_capture(record, plan, x, baseline_delay=None):
    arrays, metadata = [], []
    if len(record["files"]) < 2 or len(set(record["files"])) != len(record["files"]) or len(record.get("file_sha256", [])) != len(record["files"]):
        raise ValueError("capture requires >=2 hashed repeats")
    for index, path in enumerate(record["files"]):
        if sha(path) != record["file_sha256"][index]:
            raise ValueError(f"capture changed: {path}")
        y, rate, bits = read_wav(path)
        if (rate, bits) != (plan["sample_rate"], plan["bit_depth"]):
            raise ValueError(f"capture format mismatch: {path}")
        detected, score = pilot_delay(x, y, plan)
        if record["kind"] == "plugin":
            meta = record["render_metadata"][index]
            delay = int(meta["reported_latency_samples"])
            if (meta["sample_rate"], meta["bit_depth"]) != (rate, bits) or not meta["offline"]:
                raise ValueError("plugin render metadata mismatch")
        else:
            validate_hardware(record["settings"])
            offsets = record["settings"].get("capture_offset_samples", [0] * len(record["files"]))
            if len(offsets) != len(record["files"]):
                raise ValueError("capture_offset_samples must list one known offset per repeat")
            if any(not isinstance(v, int) for v in offsets):
                raise ValueError("capture_offset_samples must be integer sample offsets")
            delay = (detected if record["kind"] == "loopback" else baseline_delay + int(offsets[index]))
        if delay is None or delay < 0 or len(y) < delay + len(x):
            raise ValueError(f"capture truncated after latency compensation: {path}")
        aligned = y[delay:delay + len(x)]
        arrays.append(aligned)
        metadata.append(dict(path=path, delay_removed_samples=delay, pilot_detected_samples=detected,
                             transport_delay_samples=delay - (0 if record["kind"] == "plugin" else int(offsets[index])),
                             pilot_correlation=float(score), clipped_samples=int(np.count_nonzero(np.abs(y) >= 1 - 2 ** (1 - bits)))))
    if record["kind"] == "loopback" and len({m["transport_delay_samples"] for m in metadata}) != 1:
        raise ValueError("loopback latency varies across repeats; check synchronization and known capture offsets")
    return arrays, metadata


def measure_record(record, plan, x, baseline=None):
    baseline_delay = None if baseline is None else int(round(np.mean([m["transport_delay_samples"] for m in baseline["alignment"]])))
    captures, alignment = read_capture(record, plan, x, baseline_delay)
    rate = plan["sample_rate"]
    tones, individual, spectra, phase, dynamics, noise, noise_windows = [], [], [], [], [], [], []
    for s in plan["segments"]:
        if s["kind"] == "noise":
            for repeat, y in enumerate(captures):
                power = band_power(y[s["start"]:s["stop"]], rate)
                noise.append(power)
                noise_windows.append(dict(repeat=repeat + 1, start_sample=s["start"], stop_sample=s["stop"],
                                          power=power, noise_floor_dbfs_rms=float(db(math.sqrt(power)))))
        elif s["kind"] == "tone":
            a, b, f = s["analyze_start"], s["analyze_stop"], s["frequency_hz"]
            results = [tone_metrics(y[a:b], rate, f) for y in captures]
            fp = np.mean([r["fundamental_power"] for r in results])
            rp = np.mean([r["residual_power"] for r in results])
            raw_output = float(db(math.sqrt(2 * fp)))
            correction = 0
            if baseline:
                base_row = next(r for r in baseline["tones"] if r["frequency_hz"] == f and r["input_dbfs"] == s["level_dbfs"] and r["purpose"] == s["purpose"])
                correction = base_row["gain_db"]
            output = raw_output - correction
            nominal = record["settings"]["nominal_gain_db"]
            input_phase = tone_metrics(x[a:b], rate, f)["phase_rad"]
            relative_phase = np.angle(np.mean([np.exp(1j * (r["phase_rad"] - input_phase)) for r in results]))
            if baseline:
                relative_phase -= np.radians(base_row["phase_deg"])
            row = dict(frequency_hz=f, input_dbfs=s["level_dbfs"], output_dbfs=output,
                       raw_output_dbfs=raw_output, loopback_gain_correction_db=correction,
                       gain_db=output - s["level_dbfs"], gain_error_db=output - s["level_dbfs"] - nominal,
                       gain_reduction_db=nominal - (output - s["level_dbfs"]), purpose=s["purpose"],
                       output_std_db=float(np.std([db(math.sqrt(2 * r["fundamental_power"])) for r in results], ddof=1)),
                       thdn_percent=float(100 * math.sqrt(rp / max(fp, 1e-30))),
                       thdn_db=float(db(math.sqrt(rp / max(fp, 1e-30)))),
                       phase_deg=float(np.degrees(np.angle(np.exp(1j * relative_phase)))))
            for i in range(4):
                hp = [r["harmonics"][i] for r in results]
                row[f"h{i+2}_dbc"] = float(db(math.sqrt(np.mean(hp) / max(fp, 1e-30)))) if np.all(np.isfinite(hp)) else float("nan")
            tones.append(row)
            for repeat, result in enumerate(results):
                individual.append(dict(frequency_hz=f, input_dbfs=s["level_dbfs"], purpose=s["purpose"], repeat=repeat + 1,
                                       output_dbfs=float(db(math.sqrt(2 * result["fundamental_power"]))) - correction,
                                       fundamental_power=result["fundamental_power"], residual_power=result["residual_power"],
                                       fundamental_phase_deg=float(np.degrees(result["phase_rad"]))))
            if s["purpose"] == "level" and f == 1000 and s["level_dbfs"] == -18:
                n = b - a
                window = np.hanning(n)
                power = np.mean([np.abs(2 * np.fft.rfft(y[a:b] * window) / np.sum(window)) ** 2 for y in captures], axis=0)
                spectra = [dict(frequency_hz=float(freq), amplitude_dbfs=float(db(math.sqrt(p))) - correction,
                                raw_amplitude_dbfs=float(db(math.sqrt(p))), loopback_1khz_gain_correction_db=correction, bin_width_hz=rate / n)
                           for freq, p in zip(np.fft.rfftfreq(n, 1 / rate), power) if freq <= 20000]
        elif s["kind"] == "sweep":
            a, b = s["start"], s["stop"]
            transfers = [sweep_transfer(x[a:b], y[a:b], rate) for y in captures]
            frequencies = transfers[0][0]
            H = np.mean([t[1] for t in transfers], axis=0)
            if baseline:
                H = H / np.asarray([complex(r["real"], r["imag"]) for r in baseline["phase"]])
            valid = np.isfinite(H)
            unwrapped = np.full(len(H), np.nan)
            unwrapped[valid] = np.unwrap(np.angle(H[valid]))
            delay = np.full(len(H), np.nan)
            delay[valid] = -1000 * np.gradient(unwrapped[valid], frequencies[valid]) / (2 * np.pi)
            phase = [dict(frequency_hz=float(f), magnitude_db=float(db(h)), real=float(h.real), imag=float(h.imag),
                          wrapped_deg=float(np.degrees(np.angle(h))), unwrapped_deg=float(np.degrees(p)), group_delay_ms=float(g))
                     for f, h, p, g in zip(frequencies, H, unwrapped, delay)]
        elif s["kind"] == "dynamics":
            a, b = s["start"], s["stop"]
            gains = [envelope_trace(y[a:b], x[a:b], rate, s["envelope_window_ms"]) for y in captures]
            gain = np.mean(gains, axis=0)
            correction = 0 if baseline is None else next(r["gain_db"] for r in baseline["tones"] if r["frequency_hz"] == 1000 and r["input_dbfs"] == -18)
            step = max(1, rate // 1000)
            dynamics = [dict(time_ms=1000 * i / rate, gain_db=float(gain[i]) - correction,
                             gain_reduction_db=record["settings"]["nominal_gain_db"] - float(gain[i]) + correction,
                             attack_time_ms=1000 * s["attack_sample"] / rate,
                             release_time_ms=1000 * s["release_sample"] / rate) for i in range(0, len(gain), step)]
    summary = dict(id=record["id"], repeats=len(captures), noise_floor_dbfs_rms=float(db(math.sqrt(np.mean(noise)))),
                   noise_power_std=float(np.std(noise, ddof=1)), alignment=alignment,
                   clipped_samples=sum(m["clipped_samples"] for m in alignment),
                   small_signal_gain_db=next(r["gain_db"] for r in tones if r["frequency_hz"] == 1000 and r["input_dbfs"] == min(t["input_dbfs"] for t in tones)),
                   phase_1khz_deg=next(r["phase_deg"] for r in tones if r["frequency_hz"] == 1000 and r["input_dbfs"] == -18),
                   ratio=record["settings"].get("ratio"), threshold_dbfs=record["settings"].get("threshold_dbfs"),
                   makeup_gain_db=record["settings"].get("makeup_gain_db"))
    return dict(record=record, tones=tones, individual=individual, spectra=spectra, phase=phase, dynamics=dynamics,
                alignment=alignment, summary=summary, noise_windows=noise_windows)


def analyze(args):
    root, plan, x = suite(args.suite)
    records = []
    for path in args.manifests:
        manifest = load(path)
        if manifest["schema"] != SCHEMA or not manifest["complete"] or manifest["stimulus_sha256"] != plan["stimulus_sha256"]:
            raise ValueError("capture manifest incomplete or stimulus differs")
        records.extend(manifest["captures"])
    if len({r["id"] for r in records}) != len(records):
        raise ValueError("duplicate capture ids across manifests")
    if not records:
        raise ValueError("no captures to analyze")
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=False)
    measured = {}
    for record in sorted(records, key=lambda r: r["kind"] != "loopback"):
        baseline = None
        if record["kind"] == "hardware":
            baseline = measured.get(record["baseline"])
            if baseline is None or baseline["record"]["kind"] != "loopback":
                raise ValueError("hardware loopback baseline missing")
            for key in ("interface", "calibration", "capture_origin"):
                if record["settings"][key] != baseline["record"]["settings"][key]:
                    raise ValueError(f"hardware/loopback {key} differs")
        result = measure_record(record, plan, x, baseline)
        measured[record["id"]] = result
        print(f"Analyzed {record['id']}", flush=True)
    for name in ("tones", "individual", "spectra", "phase", "dynamics", "noise_windows"):
        export_csv(out / f"{name}.csv", [dict(capture_id=r["record"]["id"], model=r["record"]["model"],
                                           mode=r["record"]["mode"], oversampling=r["record"]["oversampling"], **row)
                                       for r in measured.values() for row in r[name]])
    # JSON summary excludes nonfinite chart samples; CSV uses empty fields.
    summaries = [r["summary"] for r in measured.values()]
    import importlib.metadata
    provenance = dict(python=sys.version, numpy=np.__version__, matplotlib=importlib.metadata.version("matplotlib"),
                      analyzer_sha256=sha(__file__), command=sys.argv)
    save_json(out / "summary.json", dict(schema=SCHEMA, created_utc=stamp(), analysis=provenance, plan=plan, captures=records, results=summaries))
    export_csv(out / "noise.csv", [dict(capture_id=r["id"], noise_floor_dbfs_rms=r["noise_floor_dbfs_rms"],
                                        repeats=r["repeats"], clipped_samples=r["clipped_samples"]) for r in summaries])
    configurations = []
    for record in records:
        meta = record.get("render_metadata", [{}])[0]
        configurations.append(dict(capture_id=record["id"], kind=record["kind"], model=record["model"], mode=record["mode"],
                                   selected_oversampling=record["oversampling"], effective_factor=meta.get("effective_factor", "n/a"),
                                   adaa=meta.get("adaa", "n/a"), sample_rate=plan["sample_rate"], bit_depth=plan["bit_depth"],
                                   block_size=meta.get("block_size", "n/a"), repeats=len(record["files"]),
                                   nominal_gain_db=record["settings"]["nominal_gain_db"], **meta.get("parameters", {})))
    export_csv(out / "configurations.csv", configurations)
    plot_results(measured, plan, out, load(args.targets) if args.targets else {})


def plot_results(measured, plan, out, targets):
    for key, value in targets.items():
        if key not in ("gain_error_db", "thdn_percent", "phase_difference_deg") or not isinstance(value, (float, int)) or not math.isfinite(value) or value < 0:
            raise ValueError(f"invalid measurement target: {key}")
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    plt.rcParams.update({"font.size": 9, "svg.fonttype": "none"})
    colors = dict(Standard="#2378b5", Circuit="#e48619", HQ="#299552", Hardware="#aa3377", Loopback="#777777")
    styles = {"Default": "-.", "1x": "-", "2x": "--", "4x": ":", "n/a": "-"}
    charts, differences = [], []
    conditions = (f"{plan['sample_rate']/1000:g} kHz | PCM{plan['bit_depth']} | unweighted 20 Hz–20 kHz | "
                  f"THD+N / fundamental | {'QUICK diagnostic' if plan['quick'] else 'full fixture'}")
    repeats = sorted({r["summary"]["repeats"] for r in measured.values()})
    blocks = sorted({m["block_size"] for r in measured.values() for m in r["record"].get("render_metadata", [])})
    conditions += f" | repeats {','.join(map(str, repeats))} | offline buffer {','.join(map(str, blocks)) or 'n/a'}"
    provenance = "Capture settings, repeat counts, levels, hashes and latency removal: summary.json; raw data: CSV."

    def begin(title, ylabel, xlabel, log=False):
        fig, ax = plt.subplots(figsize=(12, 6.8))
        ax.set(title=title, ylabel=ylabel, xlabel=xlabel)
        if log:
            ax.set_xscale("log")
        ax.grid(True, which="both", alpha=.2)
        return fig, ax

    def draw(ax, result, rows, xkey, ykey, label=None):
        rec = result["record"]
        controls = "; ".join(f"{k}={rec['settings'][k]}" for k in ("ratio", "threshold_dbfs", "makeup_gain_db") if k in rec["settings"])
        ax.plot([r[xkey] for r in rows], [r[ykey] for r in rows],
                color=colors[rec["mode"]], linestyle=styles[rec["oversampling"]],
                label=label or rec["label"] + ("; " + controls if controls else ""), linewidth=1.2)

    def finish(fig, ax, filename, note):
        ax.legend(loc="best", fontsize=7, ncol=3)
        fig.text(.06, .045, conditions + "\n" + note + "\n" + provenance, fontsize=7)
        fig.tight_layout(rect=(0, .13, 1, 1))
        for ext in ("png", "svg"):
            fig.savefig(out / f"{filename}.{ext}", dpi=150)
        plt.close(fig)
        charts.append((filename, note))

    for model in sorted({r["record"]["model"] for r in measured.values() if r["record"]["kind"] != "loopback"}):
        group = [r for r in measured.values() if r["record"]["model"] == model and r["record"]["kind"] != "loopback"]
        slug = model.lower().replace("-", "")
        for key, ylabel, title in [("output_dbfs", "Output fundamental (dBFS)", "Transfer / static curve"),
                                   ("gain_error_db", "Gain error from declared gain (dB)", "Linearity deviation"),
                                   ("gain_reduction_db", "Gain reduction from declared gain (dB)", "Static gain reduction / knee")]:
            fig, ax = begin(f"{model}: {title}", ylabel, "Input sine peak level (dBFS)")
            for result in group:
                rows = [r for r in result["tones"] if r["purpose"] == "level" and r["frequency_hz"] == 1000]
                draw(ax, result, rows, "input_dbfs", key)
                threshold = result["record"]["settings"].get("threshold_dbfs")
                if threshold is not None:
                    ax.axvline(threshold, color=colors[result["record"]["mode"]], alpha=.3)
            levels = [r["input_dbfs"] for r in group[0]["tones"] if r["purpose"] == "level"]
            ax.plot([min(levels), max(levels)], [min(levels), max(levels)] if key == "output_dbfs" else [0, 0], "k--", lw=.8, label="1:1" if key == "output_dbfs" else "reference")
            if key == "gain_error_db" and "gain_error_db" in targets:
                ax.axhspan(-targets["gain_error_db"], targets["gain_error_db"], color="green", alpha=.1, label="target")
            finish(fig, ax, f"{slug}-{key}", "1 kHz; actual gain retained. Hardware output level de-embedded by matched loopback gain; raw level remains in tones.csv.")
        fig, ax = begin(f"{model}: 1 kHz spectrum / harmonics", "Amplitude per FFT bin (dBFS peak)", "Frequency (Hz)", True)
        for result in group:
            draw(ax, result, result["spectra"], "frequency_hz", "amplitude_dbfs")
        for order in range(2, 6):
            ax.axvline(order * 1000, color="gray", alpha=.4)
            ax.text(order * 1000, .93, f"H{order}", transform=ax.get_xaxis_transform(), ha="center")
        ax.set_xlim(20, 20000)
        finish(fig, ax, f"{slug}-spectrum", "1 kHz, −18 dBFS; Hann, power averaged. Hardware level calibrated by loopback 1 kHz gain; raw bins/bin width in CSV. No noise subtraction.")
        for frequency in (50, 1000, 10000):
            fig, ax = begin(f"{model}: THD+N vs output — {frequency:g} Hz", "THD+N (%)", "Output fundamental (dBFS)")
            ax.set_yscale("log")
            for result in group:
                rows = [r for r in result["tones"] if r["purpose"] == "level" and r["frequency_hz"] == frequency]
                draw(ax, result, rows, "output_dbfs", "thdn_percent")
            if "thdn_percent" in targets:
                ax.axhline(targets["thdn_percent"], color="red", linestyle=":", label="limit")
            finish(fig, ax, f"{slug}-thdn-level-{frequency}", f"{frequency:g} Hz; residual RMS / fundamental RMS; out-of-band harmonics excluded. No baseline noise/distortion subtraction.")
        fig, ax = begin(f"{model}: THD+N vs frequency", "THD+N (%)", "Frequency (Hz)", True)
        ax.set_yscale("log")
        for result in group:
            draw(ax, result, [r for r in result["tones"] if r["purpose"] == "frequency"], "frequency_hz", "thdn_percent")
        finish(fig, ax, f"{slug}-thdn-frequency", "−18 dBFS sine peak; fundamental removed by sine/cosine/DC fit, residual band limited; repeats averaged as power.")
        for key, ylabel in [("wrapped_deg", "Wrapped phase (degrees)"), ("unwrapped_deg", "Unwrapped phase (degrees)"), ("group_delay_ms", "Group delay (ms)"), ("magnitude_db", "Small-signal gain (dB)")]:
            fig, ax = begin(f"{model}: {ylabel}", ylabel, "Frequency (Hz)", True)
            for result in group:
                draw(ax, result, result["phase"], "frequency_hz", key)
            finish(fig, ax, f"{slug}-{key}", "−36 dBFS logarithmic sweep; transport latency removed, DUT phase retained. Hardware complex response divided by loopback; log-bin smoothing.")
        fig, ax = begin(f"{model}: burst attack / release", "Gain reduction (dB)", "Time (ms)")
        for result in group:
            draw(ax, result, result["dynamics"], "time_ms", "gain_reduction_db")
        for key, label in [("attack_time_ms", "step up"), ("release_time_ms", "step down")]:
            ax.axvline(group[0]["dynamics"][0][key], color="gray", linestyle=":", label=label)
        finish(fig, ax, f"{slug}-dynamics", "Continuous 1 kHz tone, −36 → −6 → −36 dBFS; centered 4 ms lock-in window. Faster dynamics are unresolved; no compressor ratio assumed.")
        hardware = [r for r in group if r["record"]["kind"] == "hardware"]
        if hardware:
            fig, ax = begin(f"{model}: plugin minus hardware phase", "Phase difference (degrees)", "Frequency (Hz)", True)
            tolerance = targets.get("phase_difference_deg")
            if tolerance is not None:
                ax.axhspan(-tolerance, tolerance, color="green", alpha=.12, label=f"±{tolerance:g}° target")
            for hw in hardware:
                H = np.asarray([complex(r["real"], r["imag"]) for r in hw["phase"]])
                for result in group:
                    if result["record"]["kind"] != "plugin":
                        continue
                    P = np.asarray([complex(r["real"], r["imag"]) for r in result["phase"]])
                    difference = np.degrees(np.angle(P / H))
                    rows = [dict(frequency_hz=r["frequency_hz"], difference_deg=float(d)) for r, d in zip(result["phase"], difference)]
                    draw(ax, result, rows, "frequency_hz", "difference_deg", result["record"]["label"] + " / " + hw["record"]["label"])
                    differences.extend(dict(plugin_id=result["record"]["id"], hardware_id=hw["record"]["id"], **r,
                                            within_target="" if tolerance is None or not np.isfinite(r["difference_deg"]) else abs(r["difference_deg"]) <= tolerance) for r in rows)
            finish(fig, ax, f"{slug}-phase-difference", "Shortest signed plugin/hardware phase difference. No polarity flip or best-fit delay is removed. Targets supplied by operator.")
    # Cross-model views: every mode/rate combination gets its own axes, with
    # the four preamps overlaid. This complements the per-preamp engine views.
    plugin_results = [r for r in measured.values() if r["record"]["kind"] == "plugin"]
    model_names = [name for name in MODELS if any(r["record"]["model"] == name for r in plugin_results)]
    if len(model_names) > 1:
        selected_modes = [mode for mode in MODES if any(r["record"]["mode"] == mode for r in plugin_results)]
        selected_rates = [os for os in RATES if any(r["record"]["oversampling"] == os for r in plugin_results)]
        model_colors = dict(zip(MODELS, ["#2378b5", "#e48619", "#299552", "#aa3377"]))
        comparisons = [("transfer", "tones", "input_dbfs", "output_dbfs", "Input (dBFS)", "Output fundamental (dBFS)", False, False),
                       ("distortion", "tones", "frequency_hz", "thdn_percent", "Frequency (Hz)", "THD+N (%)", True, True),
                       ("phase", "phase", "frequency_hz", "unwrapped_deg", "Frequency (Hz)", "Unwrapped phase (degrees)", True, False),
                       ("dynamics", "dynamics", "time_ms", "gain_reduction_db", "Time (ms)", "Gain reduction (dB)", False, False)]
        for name, series, xkey, ykey, xlabel, ylabel, logx, logy in comparisons:
            fig, axes = plt.subplots(len(selected_modes), len(selected_rates), squeeze=False,
                                     figsize=(4.2 * len(selected_rates), 3 * len(selected_modes) + 2), sharex=True, sharey=True)
            fig.suptitle(f"Preamp comparison: {name} — every mode / oversampling choice", fontsize=14)
            for row_index, mode in enumerate(selected_modes):
                for column, os in enumerate(selected_rates):
                    ax = axes[row_index, column]
                    ax.set(title=f"{mode} / {os}", xlabel=xlabel if row_index == len(selected_modes) - 1 else "",
                           ylabel=ylabel if column == 0 else "")
                    if logx:
                        ax.set_xscale("log")
                    if logy:
                        ax.set_yscale("log")
                    ax.grid(True, which="both", alpha=.2)
                    for result in plugin_results:
                        rec = result["record"]
                        if rec["mode"] != mode or rec["oversampling"] != os:
                            continue
                        rows = result[series]
                        if name == "transfer":
                            rows = [r for r in rows if r["purpose"] == "level" and r["frequency_hz"] == 1000]
                        elif name == "distortion":
                            rows = [r for r in rows if r["purpose"] == "frequency"]
                        ax.plot([r[xkey] for r in rows], [r[ykey] for r in rows], color=model_colors[rec["model"]], label=rec["model"])
                    ax.legend(fontsize=7)
            fig.text(.04, .025, conditions + "\nCross-model overlays; transfer: 1 kHz, distortion: −18 dBFS, phase: −36 dBFS sweep, burst: −36/−6 dBFS.\n" + provenance, fontsize=7)
            fig.tight_layout(rect=(0, .1, 1, .96))
            filename = "all-preamps-" + name
            for ext in ("png", "svg"):
                fig.savefig(out / f"{filename}.{ext}", dpi=150)
            plt.close(fig)
            charts.append((filename, "Each panel overlays the four preamps at the same engine and selected oversampling option."))
    # Converter floor is separately visible even if no physical DUT was imported.
    fig, ax = plt.subplots(figsize=(12, max(5, len(measured) * .24 + 2)))
    ax.set(title="Measured noise floor", xlabel="Noise RMS (dBFS)", ylabel="Capture")
    ax.grid(True, axis="x", alpha=.2)
    seen_modes = set()
    for i, result in enumerate(measured.values()):
        mode = result["record"]["mode"]
        ax.barh(i, result["summary"]["noise_floor_dbfs_rms"], color=colors[mode], label=mode if mode not in seen_modes else None)
        seen_modes.add(mode)
    ax.set_yticks(range(len(measured)), [r["record"]["id"] for r in measured.values()], fontsize=7)
    finish(fig, ax, "noise-floor", "Silent windows; power averaged across independent repeats. Digital zero floored at −300 dBFS; not a physical noise-floor claim.")
    export_csv(out / "phase-difference.csv", differences)
    save_json(out / "targets.json", targets)
    checks = []
    for result in measured.values():
        for row in result["tones"]:
            for target_key, metric, absolute in (("gain_error_db", "gain_error_db", True), ("thdn_percent", "thdn_percent", False)):
                if target_key in targets:
                    value = abs(row[metric]) if absolute else row[metric]
                    checks.append(dict(capture_id=result["record"]["id"], metric=metric, frequency_hz=row["frequency_hz"],
                                       input_dbfs=row["input_dbfs"], value=value, limit=targets[target_key],
                                       passed=bool(np.isfinite(value) and value <= targets[target_key])))
    export_csv(out / "target-checks.csv", checks)
    html = ["<!doctype html><meta charset='utf-8'><title>Audio measurement comparison</title>",
            "<style>body{font:16px system-ui;margin:2rem;max-width:1300px}img{width:100%}a{color:#2378b5}</style>",
            "<h1>Audio measurement comparison</h1>", f"<p>{conditions}</p>",
            "<p>See summary.json for every setting, calibration, repeat and provenance hash. Raw CSV files accompany these charts.</p>"]
    if not any(r["record"]["kind"] == "hardware" for r in measured.values()):
        html.append("<p><strong>Plugin captures only. No physical hardware was measured in this report.</strong> Import calibrated hardware and loopback captures to add hardware overlays and phase-difference plots.</p>")
    import html as html_module
    ordered = sorted(charts, key=lambda item: not item[0].startswith("all-preamps-"))
    html.append("<nav>" + " · ".join(f"<a href='#{name}'>{html_module.escape(name)}</a>" for name, _ in ordered) + "</nav>")
    for filename, note in ordered:
        html.append(f"<figure id='{filename}'><h2>{html_module.escape(filename)}</h2><a href='{filename}.svg'><img src='{filename}.png' alt='{html_module.escape(filename)}' loading='lazy'></a><figcaption>{html_module.escape(note)}</figcaption></figure>")
    (out / "index.html").write_text("\n".join(html))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    p = commands.add_parser("generate")
    p.add_argument("out")
    p.add_argument("--rate", type=int, choices=[48000, 96000], default=48000)
    p.add_argument("--bits", type=int, choices=[16, 24, 32], default=24)
    p.add_argument("--quick", action="store_true")
    p = commands.add_parser("matrix")
    p.add_argument("--suite", required=True)
    p.add_argument("--renderer", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--models", nargs="+", choices=list(MODELS), default=list(MODELS))
    p.add_argument("--modes", nargs="+", choices=MODES, default=list(MODES))
    p.add_argument("--oversampling", nargs="+", choices=RATES, default=list(RATES))
    p.add_argument("--repeats", type=int, default=3)
    p.add_argument("--block", type=int, default=64)
    p = commands.add_parser("register")
    for arg in ("suite", "manifest", "id", "settings"):
        p.add_argument("--" + arg, required=True)
    p.add_argument("--kind", choices=["hardware", "loopback"], required=True)
    p.add_argument("--files", nargs="+", required=True)
    p.add_argument("--model", choices=list(MODELS), default="Brit")
    p.add_argument("--label")
    p.add_argument("--baseline")
    p = commands.add_parser("analyze")
    p.add_argument("--suite", required=True)
    p.add_argument("--manifests", nargs="+", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--targets", help="optional JSON: gain_error_db, thdn_percent, phase_difference_deg")
    args = parser.parse_args()
    try:
        if args.command == "generate":
            generate(args.out, args.rate, args.bits, args.quick)
        elif args.command == "matrix":
            if args.repeats < 2 or not 1 <= args.block <= 8192:
                raise ValueError("at least two repeats and block size 1..8192 required")
            capture_matrix(args)
        elif args.command == "register":
            register(args)
        elif args.command == "analyze":
            analyze(args)
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(2, f"measurement error: {error}\n")


if __name__ == "__main__":
    main()
