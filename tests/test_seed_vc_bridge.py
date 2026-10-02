"""Host-only contract tests; no model, GPU, Windows or MATLAB execution."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
import numpy as np
import soundfile as sf

spec=importlib.util.spec_from_file_location('bridge',Path(__file__).parents[1]/'python/seed_vc_bridge.py')
bridge=importlib.util.module_from_spec(spec); spec.loader.exec_module(bridge)

class BridgeTest(unittest.TestCase):
    def invoke(self,delta=0,cancel=False,bad=False):
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp); source=d/'source.wav'; reference=d/'参考.wav'; out=d/'server.wav'
            sf.write(source,np.ones(48000)*0.1,48000)
            sf.write(reference,np.ones(48000)*0.1,48000)
            sf.write(out,np.ones(22050+delta)*0.2,22050)
            job=types.SimpleNamespace(done=lambda:not cancel,result=lambda: ('stream.mp3',None if bad else {'path': str(out)}),cancel=lambda:None)
            test=self
            class Client:
                # Match the installed gradio-client 1.8.0 signature fields used here.
                def __init__(self,src,verbose,download_files,httpx_kwargs):
                    test.assertEqual(src,'http://127.0.0.1:7860')
                    test.assertIs(download_files, False)
                def submit(self,*args,api_name):
                    test.assertEqual(args[2:],(25,1.0,0.7))
                    test.assertEqual(api_name,'/predict')
                    return job
            if cancel: (d/'cancel').touch()
            with patch.dict(sys.modules,{'gradio_client':types.SimpleNamespace(Client=Client,handle_file=lambda x:x)}):
                result=bridge.convert(dict(source=str(source),reference=str(reference),steps=25,cfg=.7),d)
            samples,rate=sf.read(result['output'])
            self.assertEqual(len(samples),22050);self.assertEqual(rate,22050)
    def test_normal(self): self.invoke()
    def test_frame_padding(self): self.invoke(-100)
    def test_frame_trim(self): self.invoke(100)
    def test_truncated_rejected(self):
        with self.assertRaisesRegex(ValueError,'时长异常'): self.invoke(-5000)
    def test_cancel(self):
        with self.assertRaisesRegex(RuntimeError,'取消'): self.invoke(cancel=True)
    def test_missing_output(self):
        with self.assertRaisesRegex(RuntimeError,'完整音频'): self.invoke(bad=True)

if __name__=='__main__': unittest.main()
