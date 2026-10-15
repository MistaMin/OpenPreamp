"""End-to-end synthetic import/chart verification, never hardware evidence.

python tests/verify_measurement_report.py build/measurements/synthetic-verification
"""
import argparse
import csv
import importlib.util
from pathlib import Path
import sys
import numpy as np

spec = importlib.util.spec_from_file_location("measure_audio", Path(__file__).resolve().parents[1] / "tools/measure_audio.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


def main(root):
    root = Path(root).resolve()
    root.mkdir(parents=True, exist_ok=False)
    plan = m.generate(root / "suite", quick=True)
    _, _, x = m.suite(root / "suite")
    rng = np.random.default_rng(971)
    settings = dict(device="SYNTHETIC verification only: no physical measurement", settings={"gain": "unity"},
                    interface={"device": "SYNTHETIC"}, capture_origin="playback-synchronous", nominal_gain_db=0,
                    calibration=dict(date="2026-10-15", dac_full_scale_dbu=18, adc_full_scale_dbu=18,
                                     known_noise_floor_dbfs_rms=-110, clock="shared"))
    m.save_json(root / "settings.json", settings)
    plugin_files = []
    for kind in ("loopback", "hardware", "plugin"):
        files = []
        for repeat in range(2):
            delay = 100 if kind != "plugin" else 17
            signal = x if kind == "loopback" else -.8 * x + .1 * x ** 3
            if kind != "plugin":
                signal = .9 * signal
            signal = signal + rng.normal(0, 1e-6, len(signal))
            file = root / f"{kind}-{repeat}.wav"
            m.write_wav(file, np.r_[np.zeros(delay), signal, np.zeros(200)], 48000)
            files.append(str(file))
        if kind == "plugin":
            plugin_files = files
            continue
        m.register(argparse.Namespace(suite=root / "suite", manifest=root / "hardware.json", id=kind,
                                      settings=root / "settings.json", kind=kind, files=files, model="Brit",
                                      label=f"SYNTHETIC {kind}", baseline="loopback" if kind == "hardware" else None))
    plugin = dict(id="synthetic-plugin", label="SYNTHETIC plugin", model="Brit", mode="Circuit", oversampling="1x",
                  kind="plugin", files=plugin_files, file_sha256=[m.sha(f) for f in plugin_files], settings={"nominal_gain_db": 0},
                  render_metadata=[dict(sample_rate=48000, bit_depth=24, offline=True, block_size=64,
                                        effective_factor=1, adaa=True, reported_latency_samples=17)] * 2)
    m.save_json(root / "plugin.json", dict(schema=1, complete=True, stimulus_sha256=plan["stimulus_sha256"], captures=[plugin]))
    m.save_json(root / "targets.json", dict(gain_error_db=3, thdn_percent=2, phase_difference_deg=.05))
    m.analyze(argparse.Namespace(suite=root / "suite", manifests=[root / "hardware.json", root / "plugin.json"], out=root / "report", targets=root / "targets.json"))
    report = root / "report"
    assert len(list(report.glob("*.png"))) == len(list(report.glob("*.svg"))) == 15
    assert (report / "phase-difference.csv").exists()
    assert (report / "target-checks.csv").exists()
    with (report / "target-checks.csv").open() as f:
        assert all(row["passed"] == "True" for row in csv.DictReader(f))
    with (report / "phase-difference.csv").open() as f:
        assert all(row["within_target"] == "True" for row in csv.DictReader(f))
    assert (report / "index.html").exists()
    assert "SYNTHETIC" in (report / "brit-phase-difference.svg").read_text()
    print("PASS synthetic import, loopback correction, 15 PNG/SVG chart pairs, phase difference and CSV/HTML export")


if __name__ == "__main__":
    main(sys.argv[1])
