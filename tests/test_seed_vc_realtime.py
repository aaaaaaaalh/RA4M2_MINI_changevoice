"""Host-only checks: pinned upstream definition loading and device validation."""
import ast
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('realtime',Path(__file__).parents[1]/'python/seed_vc_realtime.py')
rt=importlib.util.module_from_spec(spec); spec.loader.exec_module(rt)

class RealtimeTests(unittest.TestCase):
    def test_callback_vad_chunk_does_not_replay_cached_audio(self):
        # Exercise the actual callback against a fake upstream which reproduces
        # the installed FunASR 1.1.5 remainder bug, without a GPU or models.
        from types import SimpleNamespace
        tree = ast.parse(Path(rt.__file__).read_text())
        callback = next(n for n in ast.walk(tree)
                        if isinstance(n, ast.FunctionDef) and n.name == 'audio_callback')
        class Base:
            def audio_callback(self, indata, outdata, frames, times, flags):
                stride = int(self.vad_chunk_size * 16000 / 1000)
                samples = self.pending + list(indata)
                remainder = len(samples) % stride
                self.processed.extend(samples[:len(samples) - remainder])
                self.pending = samples[:-remainder]  # actual 1.1.5 bug
        cls = ast.ClassDef(name='Headless', bases=[ast.Name(id='Base', ctx=ast.Load())],
                           keywords=[], body=[callback], decorator_list=[])
        namespace = dict(Base=Base, time=__import__('time'))
        exec(compile(ast.fix_missing_locations(ast.Module(body=[cls], type_ignores=[])),
                     '<callback test>', 'exec'), namespace)
        engine = namespace['Headless']()
        engine.gui_config = SimpleNamespace(samplerate=22050)
        engine.pending, engine.processed = [], []
        engine.vad_chunk_size = 500
        for block in range(1000):
            engine.audio_callback([block] * 11200, None, 15435, None, False)
            self.assertEqual(engine.pending, [])
            self.assertEqual(engine.processed, [block] * 11200)
            engine.processed.clear()

    def test_board_needs_only_output_device(self):
        d=dict(id=2,name='phones',host='MME',inputs=0,outputs=2)
        self.assertEqual(rt.validate_pair([d],dict(source_kind='board',output=d)),[d,d])

    def test_unknown_source_rejected(self):
        with self.assertRaisesRegex(RuntimeError,'源码'):
            rt.engine_tree('print("not the verified engine")')
    def test_device_identity_and_channels(self):
        devices=[dict(id=0,name='mic',host='MME',inputs=1,outputs=0),
                 dict(id=1,name='headphones',host='MME',inputs=0,outputs=2)]
        cfg=dict(input=devices[0].copy(),output=devices[1].copy())
        self.assertEqual(rt.validate_pair(devices,cfg), devices)
        cfg['input']['name']='other mic'
        with self.assertRaisesRegex(RuntimeError,'变化'):rt.validate_pair(devices,cfg)
    def test_mixed_drivers_rejected(self):
        ds=[dict(id=0,name='mic',host='MME',inputs=1,outputs=0),
            dict(id=1,name='phones',host='WASAPI',inputs=0,outputs=2)]
        with self.assertRaisesRegex(RuntimeError,'同一'):
            rt.validate_pair(ds,dict(input=ds[0],output=ds[1]))
    def test_lift_excludes_cli_and_gui_launch(self):
        source="import os\nclass Config: pass\nif __name__ == '__main__':\n    import json\n    class GUI: pass\n    gui = GUI()\n"
        with patch.object(rt,'UPSTREAM_SHA256',rt.hashlib.sha256(source.encode()).hexdigest()):
            tree=rt.engine_tree(source.replace('\n','\r\n'))
        self.assertEqual([x.name for x in tree.body if isinstance(x,ast.ClassDef)],['Config','GUI'])
        self.assertFalse(any(isinstance(x,ast.Assign) for x in tree.body))

if __name__=='__main__': unittest.main()
