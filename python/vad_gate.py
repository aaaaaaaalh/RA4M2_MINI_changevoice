"""Board-only FSMN streaming adapter; explicit events, bounded 100 ms input."""

class RelativeEnergyGate:
    """Fallback only: 2.1 s calibration, 200 ms attack, 300 ms release.

    This detects energy above background, NOT speech. It may admit other sounds.
    Calibration stays fixed for the session rather than learning speech as noise.
    """
    def __init__(self):
        self.calibration = []
        self.noise_db = None
        self.active = False
        self.attack = self.release = 0
        self.last_db = -120.0

    def step(self, samples):
        import numpy as np
        x = np.asarray(samples, dtype=np.float64)
        self.last_db = float(20 * np.log10(max(float(np.sqrt(np.mean(x*x))), 1e-6)))
        if self.noise_db is None:
            self.calibration.append(self.last_db)
            if len(self.calibration) == 21:
                self.noise_db = max(float(np.percentile(self.calibration, 75)), -70.0)
                print(f'板卡底噪标定完成：{self.noise_db:.1f} dBFS；备用门开启阈值 '
                      f'{max(self.noise_db+6, -55):.1f} dBFS', flush=True)
            return False
        threshold = max(self.noise_db + (4.5 if self.active else 6), -55)
        if self.last_db >= threshold:
            self.release = 0
            self.attack += 1
            if self.attack >= 2:
                self.active = True
        else:
            self.attack = 0
            self.release += 1
            if self.release >= 3:
                self.active = False
        return self.active


class BoardVadGate:
    def __init__(self, model, engine):
        self.model, self.engine = model, engine
        self.active = False
        self.open_for_block = False
        self.last_events = []
        self.energy = RelativeEnergyGate()
        self.frame_mask = []
        self.previous_gain = 0.0
        print("板卡备用门控：开始后请安静 2.1 秒用于底噪标定。", flush=True)

    def generate(self, input, cache, is_final=False, chunk_size=None):
        # Board blocks are 700 ms at 16 kHz: seven exact 100 ms chunks.
        # Exact chunks avoid the FunASR 1.1.5 partial-remainder bug.
        if len(input) % 1600:
            raise ValueError('板卡 VAD 输入必须由完整 100 ms 块组成。')
        opened = self.active
        energy_opened = False
        frame_mask = []
        events = []
        for offset in range(0, len(input), 1600):
            energy_active = self.energy.step(input[offset:offset+1600])
            frame_mask.append(energy_active)
            energy_opened = energy_active or energy_opened
            result = self.model.generate(input=input[offset:offset+1600], cache=cache,
                                        is_final=False, chunk_size=100)
            for start, end in result[0]['value']:
                events.append([start, end])
                if start >= 0:
                    self.active = True
                    opened = True
                if end >= 0:
                    opened = opened or self.active
                    self.active = False
        self.last_events = events
        # Neural starts are advisory: a missing end event must never bypass
        # the acoustic silence guard. Require calibrated above-noise energy.
        # Restore the two frames preceding confirmation within this block.
        self.frame_mask = frame_mask.copy()
        for i, enabled in enumerate(frame_mask):
            if enabled and (i == 0 or not frame_mask[i-1]):
                for j in range(max(0, i-2), i):
                    self.frame_mask[j] = True
        opened = any(self.frame_mask)
        self.open_for_block = opened
        self.engine.vad_speech_detected = opened
        self.engine.set_speech_detected_false_at_end_flag = not self.energy.active
        print(f"板卡门控：神经检测={bool(events) or self.active} 能量备用={energy_opened} "
              f"输入={self.energy.last_db:.1f} dBFS 标定完成={self.energy.noise_db is not None}", flush=True)
        # Upstream's parity toggle must not run after explicit state handling.
        return [{'value': []}]

    def apply_output_gate(self, output, sample_rate):
        """Mute AFTER upstream SOLA mixing; do not leave a stale audio tail.

        Input/output blocks have equal duration. Short fades avoid switching
        clicks; the 300 ms release tolerates model/crossfade alignment errors.
        No additional block buffering or change to speech rate is introduced.
        """
        import numpy as np
        if not self.frame_mask:
            output.fill(0)
            self.previous_gain = 0.0
            return
        edges = np.linspace(0, len(output), len(self.frame_mask)+1, dtype=int)
        fade = max(1, round(sample_rate * 0.005))
        for i, enabled in enumerate(self.frame_mask):
            a, b = edges[i:i+2]
            target = float(enabled)
            gains = np.full(b-a, target, dtype=np.float32)
            n = min(fade, b-a)
            if n and target != self.previous_gain:
                gains[:n] = np.linspace(self.previous_gain, target, n)
            output[a:b] *= gains.reshape((-1,) + (1,)*(output.ndim-1))
            self.previous_gain = target
        if not self.frame_mask[-1]:
            tail = getattr(self.engine, 'sola_buffer', None)
            if tail is not None:
                tail.zero_()
