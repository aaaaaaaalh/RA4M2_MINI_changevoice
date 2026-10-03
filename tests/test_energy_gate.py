import sys
from pathlib import Path
import unittest
import contextlib
import io
sys.path.insert(0, str(Path(__file__).parents[1]/'python'))
from vad_gate import RelativeEnergyGate

class EnergyTests(unittest.TestCase):
    def calibrated(self):
        g=RelativeEnergyGate()
        with contextlib.redirect_stdout(io.StringIO()):
            for _ in range(21):self.assertFalse(g.step([0.0056]*1600))
        return g
    def test_speech_rise_and_silence_release(self):
        g=self.calibrated()
        self.assertFalse(g.step([0.018]*1600))
        self.assertTrue(g.step([0.018]*1600))
        for _ in range(2):self.assertTrue(g.step([0.0056]*1600))
        self.assertFalse(g.step([0.0056]*1600))
        for _ in range(1000):self.assertFalse(g.step([0.0056]*1600))
    def test_one_chunk_impulse_does_not_open(self):
        g=self.calibrated()
        self.assertFalse(g.step([0.3]*1600))
        self.assertFalse(g.step([0.0056]*1600))
    def test_digital_silence_not_amplified(self):
        g=RelativeEnergyGate()
        with contextlib.redirect_stdout(io.StringIO()):
            for _ in range(100):self.assertFalse(g.step([0]*1600))

if __name__=='__main__':unittest.main()
