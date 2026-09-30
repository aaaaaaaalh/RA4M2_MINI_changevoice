function report=board_probe(port,seconds)
% Example: report = board_probe('COM3',5);
% Close VoiceLab acquisition and all other users of this COM port first.
if nargin<2, seconds=5; end
validateattributes(seconds,{'numeric'},{'scalar','finite','>=',1,'<=',30});
s=SerialAudioSource(port); cleanup=onCleanup(@()delete(s)); %#ok<NASGU>
x=zeros(ceil(seconds*16000/256)*256,1); wall=tic;
for k=1:256:numel(x)
    x(k:k+255)=s.readFrame(@()false,false);
end
elapsed=toc(wall); adc=x*2048+2048;
report=struct('Packets',s.Parser.Packets,'Lost',s.Parser.Lost,'BadSync',s.Parser.Bad, ...
    'ADCMean',s.ADCSum/max(1,s.ReceivedSamples),'ADCMin',s.ADCMin,'ADCMax',s.ADCMax, ...
    'MaxBlockPeakToPeak',s.PeakToPeakMax,'ClippedSamples',s.ClipSamples, ...
    'AudioSeconds',numel(x)/16000,'WallSeconds',elapsed);
disp(report);
figure('Name','Board ADC diagnosis');
subplot(2,1,1); plot((0:numel(x)-1)/16000,adc); ylim([0 4095]); ylabel('ADC counts'); xlabel('s'); grid on;
subplot(2,1,2); y=filter([1 -1],[1 -0.995],x,-x(1));
plot((0:numel(x)-1)/16000,y); ylabel('DC-removed amplitude'); xlabel('s'); grid on;
fprintf('第一幅保留原始 ADC 偏置，第二幅去直流；本测试不播放、不保存文件。\n');
end
