import csv
import gzip
from pathlib import Path
import sys
import tempfile
import unittest
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import sweep_spectrogram as sweep


def segment(rate=48000, duration=2):
    return dict(frequency_start_hz=20, frequency_stop_hz=20000, duration_s=duration,
                sweep_hz_per_s=19980 / duration, fade_seconds=.02, fft_size=512,
                hop_samples=256, level_dbfs=-6, display_min_dbfs=-140, display_max_dbfs=0,
                prediction_max_harmonic=5)


class SweepSpectrogram(unittest.TestCase):
    def test_linear_chirp_cycle_count_and_level(self):
        x = sweep.chirp(48000, 1, 100, 10000, -6, fade_seconds=0)
        crossings = np.count_nonzero((x[:-1] <= 0) & (x[1:] > 0))
        self.assertLessEqual(abs(crossings - 5050), 1)
        self.assertAlmostEqual(np.max(abs(x)), 10 ** (-6 / 20), places=5)
        with self.assertRaises(ValueError):
            sweep.chirp(48000, 1, 20, 24000, -6)

    def test_absolute_peak_bin_units_and_power_averaging(self):
        rate, n, k = 48000, 512, 37
        x = .1 * np.sin(2 * np.pi * k * np.arange(4096) / n)
        spec = sweep.spectrogram([x, -x], rate, segment())
        self.assertTrue(np.allclose(spec["power"][:, k], .01, atol=1e-14))
        self.assertTrue(np.allclose(10 * np.log10(spec["power"][:, k]), -20, atol=1e-10))
        self.assertEqual(len(spec["output_hz"]), 257)
        self.assertEqual(spec["output_hz"][-1], 24000)

    def test_predicted_harmonics_and_multiple_nyquist_folds(self):
        f = np.array([1000., 9000, 13000, 19000])
        traces = sweep.trajectories(f, 48000)
        self.assertTrue(np.array_equal(traces[2]["output_hz"], [3000, 21000, 9000, 9000]))
        self.assertTrue(np.array_equal(traces[2]["folded"], [False, True, True, True]))
        self.assertEqual(traces[4]["output_hz"][-1], 1000)

    def test_cubic_sweep_reveals_folded_third_above_20k(self):
        fs, s = 48000, segment()
        x = sweep.chirp(fs, 2, 20, 20000, -6)
        distorted = sweep.spectrogram([x + .3 * x ** 3], fs, s)
        reference = sweep.spectrogram([x], fs, s)
        i = int(np.argmin(abs(distorted["input_hz"] - 9000)))
        expected = sweep.trajectories(distorted["input_hz"], fs)[2]["output_hz"][i]
        bins = abs(distorted["output_hz"] - expected) < 500
        peak = np.max(distorted["power"][i, bins])
        floor = np.max(reference["power"][i, bins])
        self.assertGreater(10 * np.log10(peak / floor), 60)
        self.assertGreater(expected, 20000)
        self.assertGreater(10 * np.log10(peak), -50)

    def test_known_time_and_input_frequency_coordinates(self):
        s = segment()
        result = sweep.spectrogram([np.zeros(48000)], 48000, s)
        self.assertEqual(result["time_s"][0], 256 / 48000)
        self.assertEqual(result["input_hz"][0], 20 + 9990 * 256 / 48000)
        self.assertFalse(result["interior_frame"][0])
        self.assertTrue(result["interior_frame"][10])

    def test_stft_rejects_incomplete_and_nonfinite_captures(self):
        for x in (np.zeros(128), np.full(1024, np.nan)):
            with self.assertRaises(ValueError):
                sweep.spectrogram([x], 48000, segment())

    def test_96k_coordinates_and_nyquist(self):
        x = sweep.chirp(96000, 2, 20, 20000, -18)
        spec = sweep.spectrogram([x], 96000, segment(96000))
        self.assertEqual(spec["output_hz"][-1], 48000)
        self.assertEqual(spec["output_hz"][1], 187.5)

    def test_full_csv_export_roundtrip_and_safe_filenames(self):
        item = sweep.spectrogram([np.zeros(1024)], 48000, segment())
        record = {"record": {"id": "../untrusted-id"}, "linear_sweeps": [item]}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            index, tracks = sweep.export_maps({"id": record}, [item], 48000, root)
            self.assertEqual(len(index), 2)
            self.assertEqual(len(tracks), len(item["time_s"]) * 5)
            with gzip.open(root / index[0]["file"], "rt") as f:
                rows = list(csv.DictReader(f))
            self.assertEqual(len(rows), item["power"].size)
            self.assertEqual(rows[0]["capture_id"], "../untrusted-id")
            self.assertEqual(float(rows[0]["amplitude_dbfs"]), -300)


if __name__ == "__main__":
    unittest.main()
