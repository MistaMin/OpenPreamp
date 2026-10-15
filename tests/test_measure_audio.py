import argparse
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import numpy as np

spec = importlib.util.spec_from_file_location("measure_audio", Path(__file__).resolve().parents[1] / "tools/measure_audio.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class Measurements(unittest.TestCase):
    def test_pcm_roundtrip_all_formats(self):
        with tempfile.TemporaryDirectory() as d:
            for bits in (16, 24, 32):
                x = np.linspace(-1, .999, 1003)
                path = Path(d) / f"{bits}.wav"
                m.write_wav(path, x, 96000, bits)
                y, rate, actual_bits = m.read_wav(path)
                self.assertEqual((rate, actual_bits), (96000, bits))
                self.assertLessEqual(np.max(abs(x - y)), 1 / 2 ** (bits - 1))

    def test_known_harmonics_and_thdn(self):
        rate = 48000
        t = np.arange(rate) / rate
        y = .1 * np.sin(2 * np.pi * 1000 * t + .4) + .001 * np.sin(2 * np.pi * 2000 * t) + .0005 * np.sin(2 * np.pi * 3000 * t)
        r = m.tone_metrics(y, rate, 1000)
        self.assertAlmostEqual(np.sqrt(2 * r["fundamental_power"]), .1, places=10)
        self.assertAlmostEqual(r["phase_rad"], .4, places=10)
        self.assertAlmostEqual(float(m.db(np.sqrt(r["harmonics"][0] / r["fundamental_power"]))), -40, places=7)
        self.assertAlmostEqual(100 * np.sqrt(r["residual_power"] / r["fundamental_power"]), np.sqrt(1.25), places=5)

    def test_above_band_harmonics_are_unavailable(self):
        rate = 48000
        y = np.sin(2 * np.pi * 10000 * np.arange(rate) / rate)
        r = m.tone_metrics(y, rate, 10000)
        self.assertTrue(np.isfinite(r["harmonics"][0]))  # 20 kHz
        self.assertTrue(all(np.isnan(v) for v in r["harmonics"][1:]))

    def test_bandwidth_excludes_ultrasonic_noise(self):
        rate = 96000
        t = np.arange(rate) / rate
        self.assertLess(m.band_power(np.sin(2 * np.pi * 30000 * t), rate), 1e-12)
        self.assertAlmostEqual(m.band_power(np.sin(2 * np.pi * 1000 * t), rate), .5, places=7)

    def test_sweep_preserves_polarity_and_group_delay(self):
        rate = 48000
        rng = np.random.default_rng(12)
        x = np.r_[rng.normal(size=40000), np.zeros(8000)]
        y = -np.r_[np.zeros(7), x[:-7]]
        f, H = m.sweep_transfer(x, y, rate)
        valid = (f > 100) & (f < 15000)
        expected = -np.exp(-2j * np.pi * f[valid] * 7 / rate)
        self.assertLess(np.max(np.abs(np.angle(H[valid] / expected))), .04)
        delay = -1000 * np.gradient(np.unwrap(np.angle(H)), f) / (2 * np.pi)
        self.assertLess(abs(np.median(delay[valid]) - 7000 / rate), .003)

    def test_linear_burst_has_zero_gain_reduction(self):
        rate = 48000
        t = np.arange(rate) / rate
        x = .1 * np.sin(2 * np.pi * 1000 * t)
        gain = m.envelope_trace(x * .5, x, rate)
        self.assertLess(np.nanmax(abs(gain - 20 * np.log10(.5))), 1e-10)

    def test_pilot_alignment_keeps_inversion(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d) / "suite"
            plan = m.generate(root, quick=True)
            _, _, x = m.suite(root)
            delay, score = m.pilot_delay(x, -np.r_[np.zeros(137), x], plan)
            self.assertEqual(delay, 137)
            self.assertGreater(score, .99)

    def test_modified_fixture_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d) / "suite"
            m.generate(root, quick=True)
            path = root / "stimulus.wav"
            data = bytearray(path.read_bytes())
            data[-20] ^= 1
            path.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "modified"):
                m.suite(root)

    def test_hardware_requires_calibration_and_shared_clock(self):
        with self.assertRaisesRegex(ValueError, "missing"):
            m.validate_hardware({})
        settings = dict(device="test", settings={}, interface="test", capture_origin="playback-synchronous", nominal_gain_db=0,
                        calibration=dict(date="2026-10-15", dac_full_scale_dbu=18, adc_full_scale_dbu=18, known_noise_floor_dbfs_rms=-110, clock="independent"))
        with self.assertRaisesRegex(ValueError, "drift"):
            m.validate_hardware(settings)

    def test_loopback_correction_does_not_remove_dut_delay(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            plan = m.generate(root / "suite", quick=True)
            _, _, x = m.suite(root / "suite")
            settings = dict(device="synthetic test only", settings={}, interface="test", capture_origin="playback-synchronous", nominal_gain_db=0,
                            calibration=dict(date="2026-10-15", dac_full_scale_dbu=18, adc_full_scale_dbu=18, known_noise_floor_dbfs_rms=-110, clock="shared"))

            def record(kind, delay, gain):
                files = []
                for repeat in range(2):
                    file = root / f"{kind}-{repeat}.wav"
                    m.write_wav(file, np.r_[np.zeros(delay), gain * x, np.zeros(200)], 48000)
                    files.append(str(file))
                return dict(id=kind, kind=kind, files=files, file_sha256=[m.sha(f) for f in files], settings=settings)

            baseline = m.measure_record(record("loopback", 100, .5), plan, x)
            hardware = m.measure_record(record("hardware", 107, -.5), plan, x, baseline)
            self.assertEqual(hardware["alignment"][0]["delay_removed_samples"], 100)
            self.assertEqual(hardware["alignment"][0]["pilot_detected_samples"], 107)
            self.assertAlmostEqual(hardware["summary"]["small_signal_gain_db"], 0, places=3)
            # Inversion plus 7-sample DUT delay survives de-embedding.
            self.assertAlmostEqual(hardware["summary"]["phase_1khz_deg"], 127.5, places=3)
            delay = np.array([r["group_delay_ms"] for r in hardware["phase"]])
            self.assertLess(abs(np.nanmedian(delay) - 7000 / 48000), .005)

    def test_no_time_domain_noise_cancellation(self):
        rng = np.random.default_rng(17)
        noise = rng.normal(0, .001, 48000)
        average_power = np.mean([m.band_power(noise, 48000), m.band_power(-noise, 48000)])
        self.assertGreater(average_power, 5e-7)
        self.assertEqual(m.band_power((noise - noise) / 2, 48000), 0)

    def test_mismatched_format_and_truncated_capture_are_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            plan = m.generate(root / "suite", quick=True)
            _, _, x = m.suite(root / "suite")
            files = [str(root / f"capture-{i}.wav") for i in range(2)]
            record = dict(kind="plugin", files=files, render_metadata=[dict(sample_rate=48000, bit_depth=24, offline=True, reported_latency_samples=0)] * 2)
            for rate, expected in ((96000, "format"), (48000, "truncated")):
                for file in files:
                    m.write_wav(file, x[:-100], rate)
                record["file_sha256"] = [m.sha(f) for f in files]
                with self.assertRaisesRegex(ValueError, expected):
                    m.read_capture(record, plan, x)


if __name__ == "__main__":
    unittest.main()
