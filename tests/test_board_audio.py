"""Host protocol checks; no claim of serial hardware or MATLAB validation."""
import importlib.util
from pathlib import Path
import struct
import unittest
spec = importlib.util.spec_from_file_location('board', Path(__file__).parents[1]/'python/board_audio.py')
board = importlib.util.module_from_spec(spec)
spec.loader.exec_module(board)


def packet(seq, value=123):
    return b'\xaa\x55\x00\x02' + struct.pack('<H', seq) + struct.pack('<256h', *([value]*256))


class BoardTests(unittest.TestCase):
    def test_fragmented_signed_pcm_and_wrap(self):
        p = board.PacketParser()
        data = b'noise' + packet(65535, -123) + packet(0, 456)
        outputs = []
        for i in range(0, len(data), 17):
            outputs.extend(p.feed(data[i:i+17]))
        self.assertEqual(len(outputs), 2)
        self.assertEqual(struct.unpack('<256h', outputs[0]), (-123,)*256)
        self.assertEqual((p.packets, p.lost), (2, 0))
    def test_gap_duplicate_and_restart(self):
        p = board.PacketParser()
        outputs = p.feed(packet(10) + packet(12) + packet(12))
        self.assertEqual(len(outputs), 3)
        self.assertIsNone(outputs[1])
        self.assertEqual((p.lost, p.bad), (1, 1))
        with self.assertRaisesRegex(RuntimeError, '序号'):
            p.feed(packet(0))
    def test_false_sync_needs_second_header(self):
        p = board.PacketParser()
        outputs = p.feed(packet(7) + b'bad header' + packet(20) + packet(21))
        self.assertEqual(p.packets, 2)
        self.assertEqual(p.last, 21)
    def test_serial_to_output_lifecycle(self):
        import numpy as np
        import tempfile
        import sys
        from types import SimpleNamespace
        from unittest.mock import patch
        class Serial:
            def __init__(self, *args, **kwargs):
                self.data = bytearray(b''.join(packet(i, 1000 + i) for i in range(60)))
                self.closed = False
            def set_buffer_size(self, **kwargs): pass
            def reset_input_buffer(self): pass
            @property
            def in_waiting(self): return len(self.data)
            def read(self, n):
                value = bytes(self.data[:n]); del self.data[:n]; return value
            def close(self): self.closed = True
        class Output:
            def __init__(self, **kwargs): self.closed = False; self.blocks = []
            def start(self): pass
            def abort(self): pass
            def close(self): self.closed = True
            def write(self, value):
                self.blocks.append(value.copy())
                (folder/'stop').touch()
                return False
        def callback(source, output, frames, times, flags):
            self.assertEqual(source.shape, (15435, 2))
            self.assertTrue(np.isfinite(source).all())
            output[:] = source
        engine = SimpleNamespace(gui_config=SimpleNamespace(samplerate=22050, channels=2),
                                 block_frame=15435, audio_callback=callback, failure=None, xruns=0)
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            with patch.dict(sys.modules, {'serial':SimpleNamespace(Serial=Serial)}):
                stream = board.BoardStream(engine, SimpleNamespace(OutputStream=Output),
                    dict(com_port='COM3', output=dict(id=2)), folder)
                port, output = stream.port, stream.output
                stream.start(); stream.thread.join(3)
                self.assertFalse(stream.active)
                self.assertIsNone(stream.error)
                self.assertEqual(len(output.blocks), 1)
                self.assertGreaterEqual(stream.parser.packets, 44)
                stream.close()
                self.assertTrue(port.closed and output.closed)

    def test_firmware_adc_scale(self):
        import numpy as np
        adc = np.array([0, 631, 2048, 4095], dtype=np.int32)
        pcm = ((adc - 2048) * 16).astype('<i2')
        recovered = np.frombuffer(pcm.tobytes(), dtype='<i2').astype(float) / 32768
        np.testing.assert_array_equal(recovered, (adc - 2048) / 2048)

    def test_stop_while_waiting(self):
        from types import SimpleNamespace
        import threading
        stream = board.BoardStream.__new__(board.BoardStream)
        stream.np = None
        stream.samples = 0
        stream.event = threading.Event()
        stream.event.set()
        self.assertIsNone(stream.read_block(11200))

if __name__ == '__main__': unittest.main()
