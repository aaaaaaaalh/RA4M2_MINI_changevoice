"""RA4M2 serial PCM input. Wire format matches MATLAB AudioPacketParser."""
import struct
import threading
import time
from collections import deque


class PacketParser:
    def __init__(self):
        self.buffer = bytearray()
        self.locked = False
        self.last = None
        self.packets = self.lost = self.bad = 0

    def feed(self, data):
        self.buffer.extend(data)
        result = []
        while len(self.buffer) >= 6:
            b = self.buffer
            if b[:4] != b'\xaa\x55\x00\x02':
                del b[0]
                self.bad += int(self.locked)
                self.locked = False
                continue
            if len(b) < 518:
                break
            seq = struct.unpack_from('<H', b, 4)[0]
            if not self.locked:
                if len(b) < 524:
                    break
                delta = (struct.unpack_from('<H', b, 522)[0] - seq) % 65536
                if b[518:522] != b'\xaa\x55\x00\x02' or not 1 <= delta <= 257:
                    del b[0]
                    continue
                self.locked = True
            if self.last is not None:
                delta = (seq - self.last) % 65536
                if delta == 0:
                    del b[:518]
                    self.bad += 1
                    continue
                if delta > 257:
                    raise RuntimeError('板卡序号跳变，可能已复位；请停止后重新开始。')
                self.lost += delta - 1
                result.extend([None] * (delta - 1))
            result.append(bytes(b[6:518]))
            self.packets += 1
            self.last = seq
            del b[:518]
        return result


class BoardStream:
    """One serial/inference worker and a blocking output stream; bounded backlog."""
    def __init__(self, engine, sd, config, folder):
        try:
            import serial
        except ImportError as exc:
            raise RuntimeError('板卡 AI 需要 pyserial；请运行安装包中的 INSTALL_BOARD_SUPPORT.ps1。') from exc
        import numpy as np
        from scipy.signal import lfilter, resample_poly
        self.np, self.lfilter, self.resample_poly = np, lfilter, resample_poly
        self.engine, self.folder = engine, folder
        self.parser = PacketParser()
        self.pending = deque()
        self.samples = 0
        self.peak = 0.0
        self.clipped = 0
        self.gain = 10 ** (float(config.get('board_gain_db', 0)) / 20)
        self.event = threading.Event()
        self.thread = None
        self.error = None
        self.port = None
        self.output = None
        self.dc = None
        self.history = np.zeros(320, dtype=np.float32)
        try:
            self.port = serial.Serial(config['com_port'], 921600, timeout=0.1)
            self.port.set_buffer_size(rx_size=262144)
            self.port.reset_input_buffer()
            self.output = sd.OutputStream(device=config['output']['id'],
                samplerate=engine.gui_config.samplerate, channels=engine.gui_config.channels,
                dtype='float32', blocksize=engine.block_frame)
        except Exception:
            self.close()
            raise

    @property
    def active(self):
        return self.thread is not None and self.thread.is_alive() and not self.event.is_set()

    def start(self):
        self.output.start()
        self.thread = threading.Thread(target=self.work, name='VoiceLab-board', daemon=True)
        self.thread.start()

    def read_block(self, count):
        np = self.np
        deadline = time.monotonic() + 5
        while self.samples < count:
            if self.event.is_set() or (self.folder / 'stop').exists():
                return None
            waiting = self.port.in_waiting
            if waiting > 162000 or self.samples > 80000:
                raise RuntimeError('板卡音频积压超过约 5 秒，请停止后重新开始。')
            packets = self.parser.feed(self.port.read(min(max(waiting, 1), 32768)))
            for packet in packets:
                if packet is None:
                    values = np.zeros(256, dtype=np.float32)
                    self.dc = None
                else:
                    values = np.frombuffer(packet, dtype='<i2').astype(np.float32) / 32768
                    if self.dc is None:
                        self.dc = [-float(values[0])]
                    values, self.dc = self.lfilter([1, -1], [1, -0.995], values, zi=self.dc)
                self.pending.append(values)
                self.samples += len(values)
            if time.monotonic() > deadline:
                raise RuntimeError('5 秒未收到足够板卡音频，请检查 COM 口、接线和固件。')
        parts = []
        remaining = count
        while remaining:
            x = self.pending.popleft()
            take = min(remaining, len(x))
            parts.append(x[:take])
            if take < len(x):
                self.pending.appendleft(x[take:])
            remaining -= take
            self.samples -= take
        return np.concatenate(parts)

    def work(self):
        np = self.np
        import math
        e = self.engine
        rate = int(e.gui_config.samplerate)
        count = round(e.block_frame * 16000 / rate)
        divisor = math.gcd(rate, 16000)
        try:
            while not self.event.is_set() and not (self.folder / 'stop').exists():
                x = self.read_block(count)
                if x is None:
                    break
                # Keep 20 ms of filter history; pad the right edge by reflection.
                joined = np.concatenate((self.history, x))
                self.history = x[-320:].copy()
                y = self.resample_poly(joined, rate // divisor, 16000 // divisor,
                                       padtype='reflect')
                offset = round(320 * rate / 16000)
                y = y[offset:offset + e.block_frame] * self.gain
                self.peak = float(np.max(np.abs(y)))
                self.clipped += int(np.count_nonzero(np.abs(y) > 1))
                y = np.clip(y, -1, 1).astype(np.float32)
                source = np.repeat(y[:, None], e.gui_config.channels, axis=1)
                output = np.zeros_like(source)
                e.audio_callback(source, output, e.block_frame, None, False)
                if self.event.is_set():
                    break
                if self.output.write(output):
                    e.xruns += 1
        except Exception as exc:
            if not self.event.is_set():
                self.error = str(exc)
                e.failure = e.failure or ('板卡实时输入失败：' + str(exc))

    def abort(self):
        self.event.set()
        if self.output is not None:
            self.output.abort()
        if self.thread is not None:
            self.thread.join(timeout=3)

    def close(self):
        self.abort()
        if self.output is not None:
            self.output.close()
            self.output = None
        if self.port is not None:
            self.port.close()
            self.port = None

    def summary(self):
        return (f'板卡包 {self.parser.packets} · 丢包 {self.parser.lost} · '
                f'坏同步 {self.parser.bad} · 输入峰值 {self.peak:.2f} · 削顶 {self.clipped}')
