"""Local file bridge to the user's existing Seed-VC Gradio 5.23 server."""
import json
import os
from pathlib import Path
import sys
import time
import traceback


def atomic_json(path, data):
    tmp = path.with_suffix('.tmp')
    tmp.write_text(json.dumps(data, ensure_ascii=False), encoding='utf-8')
    os.replace(tmp, path)


def convert(config, directory):
    from gradio_client import Client, handle_file
    import numpy as np
    import soundfile as sf

    source = Path(config['source'])
    reference = Path(config['reference'])
    for path in (source, reference):
        if not path.is_file():
            raise ValueError(f'音频文件不存在：{path}')
    steps = int(config['steps'])
    cfg = float(config['cfg'])
    if not 1 <= steps <= 200 or not 0 <= cfg <= 1:
        raise ValueError('扩散步数或 CFG 超出范围')
    # Fixed loopback endpoint: recordings never go to a remote hosted API.
    client = Client('http://127.0.0.1:7860', verbose=False,
                    download_files=False, httpx_kwargs={'timeout': 30.0})
    job = client.submit(handle_file(str(source)), handle_file(str(reference)),
                        steps, 1.0, cfg, api_name='/predict')
    deadline = time.monotonic() + 1800
    while not job.done():
        if (directory / 'cancel').exists() or time.monotonic() > deadline:
            job.cancel()
            raise RuntimeError('已取消等待（后台当前计算可能仍需片刻结束）')
        time.sleep(0.2)
    result = job.result()
    if (directory / 'cancel').exists():
        raise RuntimeError('已取消')

    if not isinstance(result, (list, tuple)) or len(result) != 2 or not result[1]:
        raise RuntimeError('Seed-VC 未返回完整音频')

    full = result[1]
    if isinstance(full, dict):
        full = full.get('path')
    elif hasattr(full, 'path'):
        full = full.path

    if not full or not Path(full).is_file():
        raise RuntimeError(f'完整 WAV 本机路径不可读取：{full!r}')

    samples, rate = sf.read(full, dtype='float32', always_2d=True)
    samples = samples.mean(axis=1)
    if not len(samples) or not np.isfinite(samples).all():
        raise ValueError('模型输出为空或包含无效数值')
    info = sf.info(source)
    expected = round(info.frames / info.samplerate * rate)
    difference = len(samples) - expected
    # Only correct frame rounding. Never hide a truncated/model-failed output.
    if abs(difference) > round(0.1 * rate):
        raise ValueError(f'输出时长异常（差 {difference/rate:.3f} 秒），未自动裁切，请检查后台')
    samples = samples[:expected]
    if len(samples) < expected:
        samples = np.pad(samples, (0, expected-len(samples)))
    peak = float(np.max(np.abs(samples)))
    if peak > 0.999:
        samples *= 0.999 / peak
    output = directory / 'converted.wav'
    sf.write(output, samples, rate, subtype='PCM_24')
    return {'ok': True, 'output': str(output), 'sample_rate': rate,
            'duration': len(samples)/rate, 'length_correction_samples': -difference}


def main():
    config_path = Path(sys.argv[1]).resolve()
    directory = config_path.parent
    try:
        config = json.loads(config_path.read_text(encoding='utf-8-sig'))
        result = convert(config, directory)
    except Exception as exc:
        (directory/'error.log').write_text(traceback.format_exc(), encoding='utf-8')
        result = {'ok': False, 'error': str(exc)}
    atomic_json(directory/'result.json', result)
    return 0 if result['ok'] else 1


if __name__ == '__main__':
    sys.exit(main())
