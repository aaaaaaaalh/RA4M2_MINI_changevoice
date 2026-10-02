"""MATLAB control adapter for the user's installed Seed-VC realtime engine.

Loads definitions from the pinned local upstream file; never edits that file.
Seed-VC: https://github.com/Plachtaa/seed-vc (upstream license applies).
"""
import ast
import contextlib
import hashlib
import json
import os
from pathlib import Path
import sys
import time
import traceback
from types import SimpleNamespace

UPSTREAM_SHA256 = '8446147f302e5095384a510025da08575452166d8c3ff755e8e4531640ef286b'


def atomic_json(path, data):
    temp = path.with_suffix('.tmp')
    text = json.dumps(data, ensure_ascii=False)
    for attempt in range(20):
        try:
            temp.write_text(text, encoding='utf-8')
            os.replace(temp, path)
            return True
        except PermissionError:
            if attempt == 19:
                if data.get('state') == 'running':
                    print('状态文件暂时被占用，跳过本次显示更新。')
                    return False
                raise
            time.sleep(0.025)


def engine_tree(source):
    # Normalize Windows checkout line endings before checking the pinned source.
    source = source.replace('\r\n', '\n').rstrip() + '\n'
    if hashlib.sha256(source.encode('utf-8')).hexdigest() != UPSTREAM_SHA256:
        raise RuntimeError('实时源码与已核对版本不同，请提供 real-time-gui.py；未修改任何原文件。')
    tree = ast.parse(source)
    guard = next(n for n in tree.body if isinstance(n, ast.If)
                 and ast.unparse(n.test) == "__name__ == '__main__'")
    # Lift only imports/classes. Do not execute the upstream CLI or GUI launcher.
    lifted = [n for n in guard.body if isinstance(n, (ast.Import, ast.ImportFrom, ast.ClassDef))]
    tree.body = [n for n in tree.body if n is not guard] + lifted
    return ast.fix_missing_locations(tree)


def enumerate_devices():
    import sounddevice as sd
    hosts = sd.query_hostapis()
    return [dict(id=i, name=d['name'], host=hosts[d['hostapi']]['name'],
                 inputs=d['max_input_channels'], outputs=d['max_output_channels'])
            for i, d in enumerate(sd.query_devices())]


def validate_pair(devices, config):
    chosen = []
    board = config.get('source_kind') == 'board'
    fields = [('output', 'outputs')] if board else [('input', 'inputs'), ('output', 'outputs')]
    for field, channels in fields:
        item = config[field]
        match = next((d for d in devices if d['id'] == item['id']), None)
        if not match or match['name'] != item['name'] or match['host'] != item['host'] or match[channels] < 1:
            raise RuntimeError('音频设备发生变化，请刷新设备后重新选择。')
        chosen.append(match)
    if board:
        return [chosen[0], chosen[0]]
    if chosen[0]['host'] != chosen[1]['host']:
        raise RuntimeError('输入与输出必须属于同一设备驱动类型。')
    return chosen


def run(config, folder):
    print('VoiceLab 0.4.0 | VAD full-block fix | adapter:', __file__, flush=True)
    status = folder / 'status.json'
    def report(state, message, **kwargs):
        atomic_json(status, dict(state=state, message=message, **kwargs))
    if config['action'] == 'devices':
        report('devices', '设备已刷新', devices=enumerate_devices())
        return
    repo = Path(config['repo']).resolve()
    upstream = repo / 'real-time-gui.py'
    tree = engine_tree(upstream.read_text(encoding='utf-8-sig'))
    reference = Path(config['reference']).resolve()
    if not reference.is_file():
        raise RuntimeError('参考音频不存在')
    board = config.get('source_kind') == 'board'
    if board:
        import re
        if not re.fullmatch(r'COM[1-9][0-9]*', config.get('com_port', ''), re.I):
            raise ValueError('请输入有效 COM 口，例如 COM3。')
        if not -12 <= float(config.get('board_gain_db', 0)) <= 24:
            raise ValueError('板卡输入增益须在 -12 到 24 dB 之间。')
    pair = validate_pair(enumerate_devices(), config)
    os.chdir(repo)
    sys.path.insert(0, str(repo))
    report('loading', '正在加载实时模型…')
    ns = dict(__name__='voicelab_seed_runtime', __file__=str(upstream))
    exec(compile(tree, str(upstream), 'exec'), ns)
    torch = ns['torch']
    if not torch.cuda.is_available():
        raise RuntimeError('当前 Python 环境未检测到 CUDA，请使用已验证的虚拟环境。')
    ns['device'] = torch.device('cuda:0')
    base = ns['GUI']

    class Meter:
        def __init__(self):
            self.infer = None
        def __getitem__(self, key):
            return self
        def update(self, value):
            self.infer = int(value)

    class Headless(base):
        def __init__(self):
            self.gui_config = ns['GUIConfig']()
            self.config = ns['Config']()
            self.function = 'vc'
            self.stream = None
            self.window = Meter()
            self.failure = None
            self.xruns = 0
            self.last_callback = time.monotonic()
            self.model_set = ns['load_models'](SimpleNamespace(
                checkpoint_path=None, config_path=None, fp16=True))
        def get_device_channels(self):
            if board:
                return min(2, config['output']['outputs'])
            return super().get_device_channels()
        def start_stream(self):
            if not board:
                return super().start_stream()
            from board_audio import BoardStream
            ns['flag_vc'] = True
            self.stream = BoardStream(self, ns['sd'], config, folder)
            self.stream.start()
        def stop_stream(self):
            if not board:
                return super().stop_stream()
            ns['flag_vc'] = False
            if self.stream is not None:
                self.stream.close()
                self.stream = None
        def audio_callback(self, indata, outdata, frames, times, flags):
            try:
                if flags:
                    self.xruns += 1
                # FunASR 1.1.5 retains the wrong samples for a partial VAD
                # chunk (audio_sample[:-m]). Match the full incoming block
                # so m == 0, avoiding accumulated/reprocessed old audio.
                # Set here because start_vc initializes VAD to at most 500 ms.
                self.vad_chunk_size = 1000 * frames / self.gui_config.samplerate
                super().audio_callback(indata, outdata, frames, times, flags)
                self.last_callback = time.monotonic()
            except Exception:
                outdata.fill(0)
                self.failure = traceback.format_exc()
                raise ns['sd'].CallbackAbort

    engine = None
    try:
        engine = Headless()
        if (folder / 'stop').exists():
            return
        report('loading', '正在加载语音检测模型…')
        from funasr import AutoModel
        engine.vad_model = AutoModel(model='fsmn-vad', model_revision='v2.0.4')
        if (folder / 'stop').exists():
            return
        c = engine.gui_config
        c.reference_audio_path = str(reference)
        c.sg_hostapi = pair[0]['host']
        c.sg_wasapi_exclusive = False
        c.sg_input_device, c.sg_output_device = pair[0]['name'], pair[1]['name']
        c.sr_type = 'sr_model'
        c.diffusion_steps, c.inference_cfg_rate = 6, 0.7
        c.max_prompt_length, c.block_time = 3.0, 0.70
        c.crossfade_time, c.extra_time_ce = 0.04, 5.0
        c.extra_time, c.extra_time_right = 0.5, 0.02
        if not board:
            ns['sd'].default.device = [pair[0]['id'], pair[1]['id']]
        report('loading', '正在打开音频设备…')
        engine.last_callback = time.monotonic()
        engine.start_vc()
        while not (folder / 'stop').exists():
            if engine.failure:
                raise RuntimeError(engine.failure)
            if time.monotonic() - engine.last_callback > 60:
                raise RuntimeError('音频回调超过 60 秒未完成，请查看后台日志。')
            if not engine.stream.active:
                raise RuntimeError('音频流已停止，请检查设备和后台日志。')
            report('running', '实时 AI 变声运行中', infer_ms=engine.window.infer,
                   xruns=engine.xruns, sample_rate=c.samplerate,
                   board_status=engine.stream.summary() if board else '')
            time.sleep(0.5)
    finally:
        if engine is not None:
            engine.stop_stream()
    report('stopped', '实时 AI 变声已停止')


def main():
    request = Path(sys.argv[1]).resolve()
    folder = request.parent
    # Child owns its log; MATLAB never has to drain a subprocess output pipe.
    with (folder / 'backend.log').open('w', encoding='utf-8', buffering=1) as log:
        with contextlib.redirect_stdout(log), contextlib.redirect_stderr(log):
            try:
                run(json.loads(request.read_text(encoding='utf-8-sig')), folder)
                return 0
            except Exception as exc:
                traceback.print_exc()
                atomic_json(folder/'status.json', dict(state='error', message=str(exc)))
                return 1


if __name__ == '__main__':
    sys.exit(main())
