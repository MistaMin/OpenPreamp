"""Linear chirp diagnostics: explicit STFT units and harmonic/fold trajectories."""
import csv
import gzip
import math
import numpy as np


def analysis_window(size, name):
    phase = 2 * np.pi * np.arange(size) / size
    if name == "periodic 4-term Blackman-Harris":
        return .35875 - .48829 * np.cos(phase) + .14128 * np.cos(2 * phase) - .01168 * np.cos(3 * phase)
    if name == "periodic Hann":
        return .5 - .5 * np.cos(phase)
    raise ValueError("unsupported sweep analysis window")


def chirp(rate, duration, start_hz, stop_hz, level_dbfs, fade_seconds=.02):
    if rate not in (48000, 96000) or duration <= 0 or not 0 < start_hz < stop_hz < rate / 2:
        raise ValueError("invalid linear sweep")
    t = np.arange(round(rate * duration)) / rate
    phase = 2 * np.pi * (start_hz * t + .5 * (stop_hz - start_hz) / duration * t ** 2)
    x = 10 ** (level_dbfs / 20) * np.sin(phase)
    n = min(round(rate * fade_seconds), len(x) // 2)
    if n:
        x[:n] *= np.sin(np.linspace(0, np.pi / 2, n)) ** 2
        x[-n:] *= np.cos(np.linspace(0, np.pi / 2, n)) ** 2
    return x


def spectrogram(captures, rate, segment):
    n, hop = segment["fft_size"], segment["hop_samples"]
    if n < 16 or n & (n - 1) or not 1 <= hop <= n or not captures:
        raise ValueError("invalid sweep STFT settings")
    if any(len(y) != len(captures[0]) or len(y) < n or not np.all(np.isfinite(y)) for y in captures):
        raise ValueError("invalid sweep captures")
    name = segment.get("window", "periodic 4-term Blackman-Harris")
    window = analysis_window(n, name)
    powers = []
    for y in captures:
        frames = np.lib.stride_tricks.sliding_window_view(y, n)[::hop]
        powers.append(abs(2 * np.fft.rfft(frames * window, axis=1) / np.sum(window)) ** 2)
    power = np.mean(powers, axis=0)
    # Single-sided peak convention; DC and Nyquist have no mirrored partner.
    power[:, 0] *= .25
    power[:, -1] *= .25
    times = (np.arange(len(power)) * hop + n / 2) / rate
    inputs = segment["frequency_start_hz"] + segment["sweep_hz_per_s"] * times
    valid = (times >= segment["fade_seconds"] + n / (2 * rate)) & (times <= segment["duration_s"] - segment["fade_seconds"] - n / (2 * rate))
    return dict(segment=segment, time_s=times, input_hz=inputs,
                output_hz=np.fft.rfftfreq(n, 1 / rate), power=power,
                interior_frame=valid, repeats=len(captures), window=name,
                enbw_hz=float(rate * np.sum(window ** 2) / np.sum(window) ** 2))


def trajectories(inputs, rate, max_harmonic=5):
    inputs = np.asarray(inputs)
    return [dict(order=order, output_hz=np.abs((order * inputs + rate / 2) % rate - rate / 2),
                 folded=order * inputs > rate / 2) for order in range(1, max_harmonic + 1)]


def export_maps(measured, reference, rate, out):
    """Stream full-resolution long CSVs; never materialize millions of dicts."""
    index, trajectory_rows = [], []
    sources = [(r["record"]["id"], r["linear_sweeps"]) for r in measured.values() if r["linear_sweeps"]]
    sources.append(("input-reference", reference))
    for number, (ident, maps) in enumerate(sources):
        # Filenames use an ordinal: external hardware IDs never become paths.
        filename = f"linear-sweep-data-{number:03d}.csv.gz"
        with gzip.open(out / filename, "wt", newline="", compresslevel=3) as f:
            writer = csv.writer(f, lineterminator="\n")
            writer.writerow(["capture_id", "level_dbfs", "time_s", "input_hz", "output_hz", "peak_power_per_bin", "amplitude_dbfs", "interior_frame"])
            for item in maps:
                for i, (time, frequency, valid) in enumerate(zip(item["time_s"], item["input_hz"], item["interior_frame"])):
                    values = item["power"][i]
                    dbfs = 10 * np.log10(np.maximum(values, 1e-30))
                    writer.writerows((ident, item["segment"]["level_dbfs"], float(time), float(frequency), float(output),
                                      float(value), float(level), bool(valid)) for output, value, level in zip(item["output_hz"], values, dbfs))
                index.append(dict(capture_id=ident, level_dbfs=item["segment"]["level_dbfs"], file=filename,
                                  frames=len(item["time_s"]), bins=len(item["output_hz"]), repeats=item["repeats"],
                                  fft_size=item["segment"]["fft_size"], hop_samples=item["segment"]["hop_samples"],
                                  bin_width_hz=rate / item["segment"]["fft_size"], window=item["window"], enbw_hz=item["enbw_hz"],
                                  sweep_hz_per_s=item["segment"]["sweep_hz_per_s"],
                                  display_min_dbfs=item["segment"]["display_min_dbfs"], display_max_dbfs=item["segment"]["display_max_dbfs"]))
    for item in reference:
        for trace in trajectories(item["input_hz"], rate, item["segment"]["prediction_max_harmonic"]):
            trajectory_rows.extend(dict(level_dbfs=item["segment"]["level_dbfs"], time_s=float(t), input_hz=float(x),
                                        order=trace["order"], output_hz=float(y), folded=bool(folded))
                                   for t, x, y, folded in zip(item["time_s"], item["input_hz"], trace["output_hz"], trace["folded"]))
    return index, trajectory_rows


def plot_maps(measured, reference, plan, out):
    import matplotlib.pyplot as plt
    from matplotlib.lines import Line2D
    charts = []
    rate = plan["sample_rate"]
    colors = ("white", "#ffda66", "#7ae5ff", "#88ef9c", "#f7a7ed")

    def overlay(ax, inputs, max_harmonic):
        for trace, color in zip(trajectories(inputs, rate, max_harmonic), colors):
            for folded, style in ((False, "-"), (True, "--")):
                outputs = np.where(trace["folded"] == folded, trace["output_hz"], np.nan)
                ax.plot(inputs / 1000, outputs / 1000, color=color, linestyle=style, lw=.55, alpha=.65)

    def figure(entries, level, filename, title, annotated):
        columns = min(3, len(entries))
        rows = math.ceil(len(entries) / columns)
        fig, axes = plt.subplots(rows, columns, squeeze=False, sharex=True, sharey=True,
                                 figsize=(max(12, 4.5 * columns + 1), 3.15 * rows + 2.7))
        spec = entries[0][1]["segment"]
        for i, (label, item) in enumerate(entries):
            ax = axes.flat[i]
            power_db = 10 * np.log10(np.maximum(item["power"], 1e-30))
            image = ax.pcolormesh(item["input_hz"] / 1000, item["output_hz"] / 1000, power_db.T,
                                  cmap="magma", vmin=spec["display_min_dbfs"], vmax=spec["display_max_dbfs"],
                                  shading="nearest", rasterized=True)
            if annotated:
                overlay(ax, item["input_hz"], spec["prediction_max_harmonic"])
            ax.set(title=label, xlim=(spec["frequency_start_hz"] / 1000, spec["frequency_stop_hz"] / 1000), ylim=(0, rate / 2000))
            if i // columns == rows - 1:
                ax.set_xlabel("Input sweep frequency (kHz; linear)")
            if i % columns == 0:
                ax.set_ylabel("Output frequency (kHz)")
        for ax in list(axes.flat)[len(entries):]:
            ax.set_visible(False)
        fig.suptitle(title + f" — {level} dBFS sine peak", fontsize=13, y=.98)
        if annotated:
            handles = [Line2D([], [], color=c, label=f"H{i+1}") for i, c in enumerate(colors)]
            handles.append(Line2D([], [], color="gray", linestyle="--", label="dashed = predicted fold"))
            fig.legend(handles=handles, loc="upper center", bbox_to_anchor=(.47, .945), ncol=6, fontsize=8, facecolor="#333333", labelcolor="white")
        top = .84 if annotated else .88
        fig.subplots_adjust(left=.07, right=.86, bottom=.20, top=top, hspace=.28, wspace=.12)
        cax = fig.add_axes((.89, .23, .015, top - .25))
        fig.colorbar(image, cax=cax, label="STFT amplitude (dBFS peak / bin)")
        n, hop, slope = spec["fft_size"], spec["hop_samples"], spec["sweep_hz_per_s"]
        repeats = sorted({item["repeats"] for _, item in entries})
        foot = (f"{rate/1000:g} kHz | PCM{plan['bit_depth']} | 20 Hz → 20 kHz linear chirp in {spec['duration_s']:g} s | {slope:g} Hz/s | repeats {repeats} averaged as power\n"
                f"{entries[0][1]['window']} N={n}, hop={hop}; Δf={rate/n:.2f} Hz, ENBW={entries[0][1]['enbw_hz']:.2f} Hz, window={1000*n/rate:.2f} ms; drift/window H1={slope*n/rate:.1f}, H5={5*slope*n/rate:.1f} Hz\n"
                f"Display: 0–Nyquist, fixed {spec['display_min_dbfs']}…{spec['display_max_dbfs']} dBFS; no gain normalization, de-embedding or noise subtraction. Raw calibrated ADC levels for hardware.\n"
                "Lines predict positions only; STFT brightness is not an isolated harmonic amplitude/THD estimate. Fades/edges retained and flagged in CSV.\n"
                "Transport latency removed; DUT delay/phase retained. Settings/buffer/ADAA in summary.json; full maps in linear-sweep-index.csv and CSV.gz.")
        fig.text(.055, .025, foot, fontsize=7)
        for extension in ("png", "svg"):
            fig.savefig(out / f"{filename}.{extension}", dpi=150)
        plt.close(fig)
        charts.append((filename, f"{title}: {level} dBFS, common absolute color scale. " + ("H1–H5 solid; predicted aliases dashed." if annotated else "Clean measured map; no trajectory overlays.")))

    for reference_map in reference:
        level = reference_map["segment"]["level_dbfs"]
        figure([("Quantized input fixture", reference_map)], level, f"linear-sweep-input-{abs(level)}", "Linear sweep input reference", False)
    models = sorted({r["record"]["model"] for r in measured.values() if r["record"]["kind"] != "loopback" and r["linear_sweeps"]})
    for model in models:
        group = [r for r in measured.values() if r["record"]["model"] == model and r["record"]["kind"] != "loopback"]
        for reference_map in reference:
            level = reference_map["segment"]["level_dbfs"]
            entries = [(r["record"]["label"], item) for r in group for item in r["linear_sweeps"] if item["segment"]["level_dbfs"] == level]
            if not entries:
                continue
            slug = model.lower().replace("-", "")
            for annotated in (False, True):
                suffix = "-guide" if annotated else ""
                figure(entries, level, f"linear-sweep-{slug}-{abs(level)}{suffix}", f"{model}: linear sweep harmonic / alias map", annotated)
    for result in measured.values():
        if result["record"]["kind"] == "loopback":
            for i, item in enumerate(result["linear_sweeps"]):
                figure([(result["record"]["label"], item)], item["segment"]["level_dbfs"],
                       f"linear-sweep-loopback-{len(charts)}-{i}", "Loopback linear sweep reference", False)
    return charts
