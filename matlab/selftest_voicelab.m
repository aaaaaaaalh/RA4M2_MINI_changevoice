function selftest_voicelab
% Device-free tests. Run in MATLAB R2025b before opening the app.
rng(7); fs=16000; p=VoiceProcessor.defaults();
t=(0:fs*2-1)'/fs; x=0.15*sin(2*pi*440*t);
% OLA reconstruction with zero noise must be exact after 256-sample alignment.
p.Denoise=true; e=VoiceProcessor(p,zeros(512,1),false);
z=[x;zeros(256,1)]; d=zeros(size(z));
for k=1:256:numel(z), d(k:k+255)=e.denoise(z(k:k+255)); end
delete(e); assert(max(abs(d(257:end)-x))<1e-10,'OLA reconstruction failed');
% Learned stationary noise should be attenuated on an independent noise sample.
noise=VoiceProcessor.learn(0.04*randn(fs,1));
n=0.04*randn(fs,1); y=VoiceProcessor.offline(n,p,noise);
assert(numel(y)==numel(n) && all(isfinite(y)),'Invalid output');
assert(rms(y)<0.8*rms(n),'Noise attenuation failed');
% Up/down pitch must change frequency, not duration.
p.Denoise=false;
for shift=[-5 5]
    p.Semitones=shift; y=VoiceProcessor.offline(x,p,[]);
    assert(numel(y)==numel(x),'Duration changed');
    clip=y(4001:24000); sp=abs(fft(clip.*hann(numel(clip))));
    [~,bin]=max(sp(1:floor(end/2))); f=(bin-1)*fs/numel(clip);
    target=440*2^(shift/12);
    assert(abs(f-target)<8,'Incorrect shifted tone');
end
% 1 kHz unit sine should be approximately 0 dBFS, and silence -100 dBFS.
b=VoiceProcessor.spectrum(sin(2*pi*1000*(0:511)'/fs));
assert(abs(max(b))<0.1 && all(VoiceProcessor.spectrum(zeros(512,1))==-100));
% Robot modulation should produce both sidebands.
p=VoiceProcessor.defaults(); p.Robot=true; p.Carrier=130;
y=VoiceProcessor.offline(x,p,[]); sp=abs(fft(y(fs+1:end)));
assert(sp(311)>100 && sp(571)>100,'Robot sidebands absent');
% Very short and odd-length clips retain length.
for n=[1 255 257 5001]
    p=VoiceProcessor.defaults(); p.Denoise=true;
    y=VoiceProcessor.offline(zeros(n,1),p,noise);
    assert(numel(y)==n && all(isfinite(y)));
end
% Streaming pitch example must exist and accept the configured presets.
for semi=[-3.5 3.5 6]
    p=VoiceProcessor.defaults(); p.Semitones=semi;
    e=VoiceProcessor(p,[],true);
    for k=1:20, y=e.step(x(1:256)); assert(numel(y)==256 && all(isfinite(y))); end
    delete(e);
end
fprintf('VoiceLab device-free tests passed. Audio devices and UI require manual testing.\n');
end
