import sys
from pathlib import Path
import unittest
import contextlib
import io
import numpy as np
from types import SimpleNamespace
sys.path.insert(0, str(Path(__file__).parents[1]/'python'))
from vad_gate import BoardVadGate

class Fake:
    def __init__(self, events=()): self.events=iter(events); self.calls=0
    def generate(self, input, **kwargs):
        assert len(input)==1600 and kwargs['chunk_size']==100
        self.calls+=1
        return [{'value':next(self.events, [])}]

class Tail:
    def __init__(self): self.cleared=False
    def zero_(self): self.cleared=True

class GateTests(unittest.TestCase):
    def setUp(self):
        self.quiet=contextlib.redirect_stdout(io.StringIO()); self.quiet.__enter__()
        self.addCleanup(self.quiet.__exit__, None, None, None)
    def gate(self):
        e=SimpleNamespace(sola_buffer=Tail()); m=Fake(); g=BoardVadGate(m,e)
        for _ in range(3): g.generate(np.full(11200,.0056),{})
        return g,e,m
    def test_neural_start_without_end_cannot_hold_silence_open(self):
        g,e,m=self.gate();g.model=Fake([[[0,-1]]])
        for _ in range(100):
            g.generate(np.full(11200,.0056),{})
            self.assertFalse(e.vad_speech_detected)
            out=np.ones((15435,2));g.apply_output_gate(out,22050)
            self.assertFalse(out.any())
    def test_weak_speech_works_without_neural_events_then_mutes(self):
        g,e,m=self.gate();g.generate(np.full(11200,.018),{})
        self.assertTrue(e.vad_speech_detected)
        self.assertTrue(g.frame_mask[0]) # recovered attack frames
        out=np.ones((15435,2));g.apply_output_gate(out,22050)
        self.assertGreater(out.mean(),.98)
        g.generate(np.full(11200,.0056),{})
        out=np.ones((15435,2));g.apply_output_gate(out,22050)
        self.assertFalse(out[7000:].any())
        self.assertTrue(e.sola_buffer.cleared)
        self.assertTrue(e.set_speech_detected_false_at_end_flag)
    def test_subthreshold_noise_after_speech_cannot_latch(self):
        g,e,_=self.gate();g.generate(np.full(11200,.018),{})
        # 4 dB above learned background, below the release threshold.
        for _ in range(10):g.generate(np.full(11200,.0056*10**(4/20)),{})
        self.assertFalse(e.vad_speech_detected)
    def test_calibration_mutes_even_neural_events(self):
        e=SimpleNamespace();g=BoardVadGate(Fake([[[0,-1]]]),e)
        g.generate(np.full(11200,.0056),{})
        out=np.ones((15435,2));g.apply_output_gate(out,22050)
        self.assertFalse(out.any())
        self.assertFalse(e.vad_speech_detected)
    def test_incomplete_chunk_rejected(self):
        g,_,_=self.gate()
        with self.assertRaises(ValueError):g.generate([0]*11201,{})

if __name__=='__main__':unittest.main()
