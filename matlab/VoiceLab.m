classdef VoiceLab < handle
    % Run: app = VoiceLab;
    properties
        Fig
        Controls
        Status
        ModeText
        InputDevice
        OutputDevice
        Preset
        Pitch
        Bright
        Carrier
        Gain
        Denoise
        Alpha
        Formants
        WaveIn
        WaveOut
        SpecIn
        SpecOut
        NoiseText
        StopButton
        Busy=false
        StopRequested=false
        Closing=false
        Mode='offline'
        Raw=[]
        Output=[]
        Noise=[]
        Player=[]
        Fs=16000
        H=256
        MaxSeconds=300
    end
    methods
        function obj=VoiceLab()
            required={'audioDeviceReader','audioDeviceWriter','shiftPitch','butter'};
            for k=1:numel(required)
                assert(~isempty(which(required{k})),['缺少函数或工具箱：' required{k}]);
            end
            obj.build(); obj.devices();
        end
        function build(obj)
            obj.Fig=uifigure('Name','VoiceLab v0.3 · 板卡语音实验室','Position',[80 80 1220 820], ...
                'Color',[0.95 0.96 0.98],'CloseRequestFcn',@(~,~)obj.close());
            root=uigridlayout(obj.Fig,[4 1]); root.RowHeight={52,44,'1x',46};
            heading=uilabel(root,'Text','VoiceLab  /  语音变声实验室','FontSize',24,'FontWeight','bold');
            heading.FontColor=[0.10 0.19 0.32];
            modes=uigridlayout(root,[1 4]); modes.ColumnWidth={150,150,190,'1x'};
            a=uibutton(modes,'Text','实时变声','ButtonPushedFcn',@(~,~)obj.setMode('live'));
            b=uibutton(modes,'Text','录音后变声','ButtonPushedFcn',@(~,~)obj.setMode('offline'));
            ai=uibutton(modes,'Text','AI 参考音色变声','ButtonPushedFcn',@(~,~)VoiceLabAI(obj.Raw,obj.Fs));
            obj.ModeText=uilabel(modes,'Text','当前：录音后变声 · 16 kHz / 单声道');
            body=uigridlayout(root,[1 2]); body.ColumnWidth={340,'1x'};
            panel=uipanel(body,'Title','音频与效果设置');
            c=uigridlayout(panel,[19 2]); c.ColumnWidth={115,'1x'};
            c.RowHeight=repmat({30},1,19); c.Scrollable='on';
            obj.InputDevice=obj.drop(c,1,'输入设备',{'默认'});
            obj.OutputDevice=obj.drop(c,2,'输出设备',{'默认'});
            obj.InputDevice.ValueChangedFcn=@(~,~)obj.invalidateNoise();
            refresh=uibutton(c,'Text','刷新设备','ButtonPushedFcn',@(~,~)obj.devices()); refresh.Layout.Row=3; refresh.Layout.Column=1;
            learn=uibutton(c,'Text','学习环境噪声（1秒）','ButtonPushedFcn',@(~,~)obj.capture('noise')); learn.Layout.Row=3; learn.Layout.Column=2;
            obj.Preset=obj.drop(c,4,'音色风格',{'原声','女声风格','男声风格','老人','儿童','机器人'});
            obj.Preset.ValueChangedFcn=@(~,~)obj.presetChanged();
            obj.Pitch=obj.numeric(c,5,'音高 / 半音',[-12 12],0);
            obj.Bright=obj.numeric(c,6,'明亮度 / dB',[-12 12],0);
            obj.Carrier=obj.numeric(c,7,'机器人频率 / Hz',[50 400],130);
            obj.Gain=obj.numeric(c,8,'输出增益 / dB',[-24 12],0);
            obj.Denoise=uicheckbox(c,'Text','开启谱减降噪','Value',false); obj.Denoise.Layout.Row=9; obj.Denoise.Layout.Column=[1 2];
            obj.Alpha=obj.numeric(c,10,'降噪强度 α',[0.5 3],1.5);
            obj.Formants=uicheckbox(c,'Text','离线：保留共振峰（偏自然）','Value',false); obj.Formants.Layout.Row=11; obj.Formants.Layout.Column=[1 2];
            obj.NoiseText=uilabel(c,'Text','未学习噪声；开启降噪前请先学习。','FontSize',11); obj.NoiseText.Layout.Row=12; obj.NoiseText.Layout.Column=[1 2];
            start=uibutton(c,'Text','开始实时 / 开始录音','ButtonPushedFcn',@(~,~)obj.start()); start.Layout.Row=13; start.Layout.Column=[1 2];
            obj.StopButton=uibutton(c,'Text','■ 停止 / 取消','ButtonPushedFcn',@(~,~)obj.stop()); obj.StopButton.Layout.Row=14; obj.StopButton.Layout.Column=[1 2];
            imp=uibutton(c,'Text','导入音频','ButtonPushedFcn',@(~,~)obj.importAudio()); imp.Layout.Row=15; imp.Layout.Column=1;
            process=uibutton(c,'Text','执行离线变声','ButtonPushedFcn',@(~,~)obj.process()); process.Layout.Row=15; process.Layout.Column=2;
            play1=uibutton(c,'Text','播放原声','ButtonPushedFcn',@(~,~)obj.playAudio(false)); play1.Layout.Row=16; play1.Layout.Column=1;
            play2=uibutton(c,'Text','播放处理结果','ButtonPushedFcn',@(~,~)obj.playAudio(true)); play2.Layout.Row=16; play2.Layout.Column=2;
            save1=uibutton(c,'Text','导出原声 WAV','ButtonPushedFcn',@(~,~)obj.saveAudio(false)); save1.Layout.Row=17; save1.Layout.Column=1;
            save2=uibutton(c,'Text','导出结果 WAV','ButtonPushedFcn',@(~,~)obj.saveAudio(true)); save2.Layout.Row=17; save2.Layout.Column=2;
            note=uilabel(c,'Text','实时监听请戴耳机；参数在开始前设置。','FontSize',11); note.Layout.Row=18; note.Layout.Column=[1 2];
            note=uilabel(c,'Text','单次最长 5 分钟；实时结果保留算法延迟。','FontSize',11); note.Layout.Row=19; note.Layout.Column=[1 2];
            plots=uigridlayout(body,[2 2]);
            ax=uiaxes(plots); title(ax,'原声 · 时域'); xlabel(ax,'时间 / s'); ylabel(ax,'幅度'); obj.WaveIn=plot(ax,0,0,'Color',[0.12 .45 .75]); ylim(ax,[-1 1]);
            ax=uiaxes(plots); title(ax,'处理后 · 时域'); xlabel(ax,'时间 / s'); ylabel(ax,'幅度'); obj.WaveOut=plot(ax,0,0,'Color',[.1 .6 .45]); ylim(ax,[-1 1]);
            ax=uiaxes(plots); title(ax,'原声 · 32 柱频谱'); obj.SpecIn=bar(ax,125:250:7875,-100*ones(1,32),1,'FaceColor',[.12 .45 .75],'BaseValue',-100); obj.spectrumAxes(ax);
            ax=uiaxes(plots); title(ax,'处理后 · 32 柱频谱'); obj.SpecOut=bar(ax,125:250:7875,-100*ones(1,32),1,'FaceColor',[.1 .6 .45],'BaseValue',-100); obj.spectrumAxes(ax);
            obj.Status=uilabel(root,'Text','就绪 · 先选择输入输出设备。','WordWrap','on');
            obj.Controls={a,b,ai,obj.InputDevice,obj.OutputDevice,refresh,learn,obj.Preset,obj.Pitch,obj.Bright,obj.Carrier,obj.Gain,obj.Denoise,obj.Alpha,obj.Formants,start,imp,process,play1,play2,save1,save2};
        end
        function d=drop(~,grid,row,label,items)
            l=uilabel(grid,'Text',label); l.Layout.Row=row; l.Layout.Column=1;
            d=uidropdown(grid,'Items',items); d.Layout.Row=row; d.Layout.Column=2;
        end
        function d=numeric(~,grid,row,label,limits,value)
            l=uilabel(grid,'Text',label); l.Layout.Row=row; l.Layout.Column=1;
            d=uieditfield(grid,'numeric','Limits',limits,'Value',value); d.Layout.Row=row; d.Layout.Column=2;
        end
        function spectrumAxes(~,ax)
            xlim(ax,[0 8000]); ylim(ax,[-100 6]); xlabel(ax,'频率 / Hz'); ylabel(ax,'近似 dBFS');
        end
        function devices(obj)
            r=[]; w=[];
            try
                r=audioDeviceReader; w=audioDeviceWriter;
                ins=cellstr(getAudioDevices(r)); outs=cellstr(getAudioDevices(w));
                ports=cellstr(serialportlist('available'));
                boards=cellfun(@(v)['板卡串口 ' v],ports,'UniformOutput',false);
                obj.InputDevice.Items=[{'默认'},reshape(boards,1,[]),reshape(ins,1,[])];
                obj.OutputDevice.Items=[{'默认'},reshape(outs,1,[])];
                obj.invalidateNoise();
            catch e, obj.fail(e);
            end
            if ~isempty(r), release(r); end
            if ~isempty(w), release(w); end
        end
        function invalidateNoise(obj)
            obj.Noise=[]; obj.NoiseText.Text='输入设备已改变，请重新学习噪声。';
        end
        function setMode(obj,mode)
            if obj.Busy, return; end
            obj.Mode=mode;
            if strcmp(mode,'live'), s='实时变声'; else, s='录音后变声'; end
            obj.ModeText.Text=['当前：' s ' · 16 kHz / 单声道'];
        end
        function presetChanged(obj)
            switch obj.Preset.Value
                case '女声风格', v=[3.5 4];
                case '男声风格', v=[-3.5 -4];
                case '老人', v=[-1.5 -3];
                case '儿童', v=[6 5];
                otherwise, v=[0 0];
            end
            obj.Pitch.Value=v(1); obj.Bright.Value=v(2);
        end
        function p=params(obj)
            p=VoiceProcessor.defaults();
            p.Semitones=obj.Pitch.Value; p.Brightness=obj.Bright.Value; p.Carrier=obj.Carrier.Value;
            p.Gain=obj.Gain.Value; p.Denoise=obj.Denoise.Value; p.Alpha=obj.Alpha.Value;
            p.PreserveFormants=obj.Formants.Value;
            p.Robot=strcmp(obj.Preset.Value,'机器人'); p.Old=strcmp(obj.Preset.Value,'老人');
            if p.Old, p.Highpass=180; p.Lowpass=4200; end
            if strcmp(obj.Preset.Value,'男声风格'), p.Lowpass=5500; end
            if any(strcmp(obj.Preset.Value,{'女声风格','儿童'})), p.Highpass=120; end
            if p.Denoise && isempty(obj.Noise), error('请先点击“学习环境噪声”，并保持安静 1 秒。'); end
        end
        function start(obj)
            if strcmp(obj.Mode,'live'), obj.capture('live'); else, obj.capture('record'); end
        end
        function capture(obj,kind)
            if obj.Busy, return; end
            r=[]; w=[]; engine=[]; success=false; count=0; serialInput=false;
            try
                live=strcmp(kind,'live'); noise=strcmp(kind,'noise');
                if live, p=obj.params(); engine=VoiceProcessor(p,obj.Noise,true); end
                obj.setBusy(true);
                cleanup=onCleanup(@()obj.finish()); %#ok<NASGU>
                serialInput=startsWith(obj.InputDevice.Value,'板卡串口 ');
                if serialInput
                    r=SerialAudioSource(extractAfter(obj.InputDevice.Value,'板卡串口 '));
                else
                    r=audioDeviceReader('SampleRate',obj.Fs,'SamplesPerFrame',obj.H,'NumChannels',1);
                    if ~strcmp(obj.InputDevice.Value,'默认'), r.Device=obj.InputDevice.Value; end
                end
                if live
                    w=audioDeviceWriter('SampleRate',obj.Fs);
                    if ~strcmp(obj.OutputDevice.Value,'默认'), w.Device=obj.OutputDevice.Value; end
                end
                seconds=obj.MaxSeconds; if noise, seconds=1; end
                frames=ceil(seconds*obj.Fs/obj.H);
                raw=zeros(frames*obj.H,1); out=zeros(size(raw));
                over=0; under=0; last=tic;
                if noise, obj.Status.Text='正在学习噪声：保持安静，不要说话。'; else, obj.Status.Text='正在采集；点击停止结束并保留音频。'; end
                drawnow;
                while ~obj.StopRequested && count<frames
                    if serialInput
                        x=r.readFrame(@()obj.StopRequested);
                        if isempty(x), break; end
                        over=r.Parser.Lost*obj.H;
                    else
                        [x,ov]=r(); over=over+double(ov);
                    end
                    if live, y=engine.step(x); under=under+double(w(y)); else, y=x; end
                    count=count+1; idx=(count-1)*obj.H+(1:obj.H); raw(idx)=x; out(idx)=y;
                    if toc(last)>0.15
                        a=max(1,idx(end)-obj.Fs+1);
                        obj.drawAudio(raw(a:idx(end)),out(a:idx(end)));
                        if ~noise
                            obj.Status.Text=sprintf('采集 %.1f 秒 | 输入丢失 %d 样本 | 输出欠载 %d 样本 | RMS %.1f → %.1f dBFS',count*obj.H/obj.Fs,over,under,obj.level(x),obj.level(y));
                        end
                        if serialInput && ~noise
                            obj.Status.Text=sprintf('%.1fs | %s | 欠载 %d | RMS %.1f → %.1f dBFS', ...
                                count*obj.H/obj.Fs,r.summary(),under,obj.level(x),obj.level(y));
                        end
                        last=tic;
                    end
                    drawnow limitrate;
                end
                if count>0
                    raw=raw(1:count*obj.H); out=out(1:count*obj.H);
                    if noise
                        if obj.StopRequested, error('VoiceLab:Cancelled','噪声学习已取消，保留旧噪声模型'); end
                        obj.Noise=VoiceProcessor.learn(raw);
                        obj.NoiseText.Text='噪声模型已学习；更换设备/环境后请重学。';
                    else
                        obj.Raw=raw; obj.Output=[];
                        if live, obj.Output=out; end
                    end
                end
                success=true;
            catch e, obj.fail(e);
            end
            if serialInput && ~isempty(r)
                serialSummary=r.summary();
                if ~success && count>0 && ~strcmp(kind,'noise')
                    obj.Raw=raw(1:count*obj.H); obj.Output=[];
                    if strcmp(kind,'live'), obj.Output=out(1:count*obj.H); end
                end
            end
            if ~isempty(r), release(r); end
            if ~isempty(w), release(w); end
            if ~isempty(engine), delete(engine); end
            if success && ~obj.Closing
                obj.Status.Text=sprintf('完成：%.2f 秒。输入丢失 %d / 输出欠载 %d 样本。',count*obj.H/obj.Fs,over,under);
                if serialInput, obj.Status.Text=['完成 | ' serialSummary]; end
            end
        end
        function importAudio(obj)
            if obj.Busy, return; end
            [f,p]=uigetfile({'*.wav;*.mp3;*.flac;*.m4a','音频文件'});
            if isequal(f,0), return; end
            try
                obj.stopPlayback(); info=audioinfo(fullfile(p,f));
                if info.Duration>obj.MaxSeconds, error('本版最多导入 5 分钟音频，请先截取。'); end
                [x,fs]=audioread(fullfile(p,f)); x=mean(x,2);
                if fs~=obj.Fs, x=resample(x,obj.Fs,fs); end
                if isempty(x)||any(~isfinite(x)), error('音频为空或包含无效样本。'); end
                obj.Raw=x; obj.Output=[]; obj.drawAudio(x,zeros(size(x)));
                obj.Status.Text=sprintf('已导入 %s，%.2f 秒；转换为 16 kHz 单声道。',f,numel(x)/obj.Fs);
            catch e, obj.fail(e); end
        end
        function process(obj)
            if obj.Busy, return; end
            try
                if isempty(obj.Raw), error('请先录音或导入音频。'); end
                p=obj.params(); obj.setBusy(true); cleanup=onCleanup(@()obj.finish()); %#ok<NASGU>
                y=VoiceProcessor.offline(obj.Raw,p,obj.Noise,@(v)obj.progress(v));
                obj.Output=y; obj.drawAudio(obj.Raw,y);
                obj.Status.Text=sprintf('离线处理完成 | 时长 %.3f 秒 | RMS %.1f → %.1f dBFS',numel(y)/obj.Fs,obj.level(obj.Raw),obj.level(y));
            catch e, obj.fail(e); end
        end
        function ok=progress(obj,v)
            obj.Status.Text=sprintf('离线处理 %.0f%%；变调计算期间取消将在当前计算结束后生效。',100*v);
            drawnow; ok=~obj.StopRequested;
        end
        function playAudio(obj,processed)
            if obj.Busy, return; end
            x=obj.Raw; if processed, x=obj.Output; end
            if isempty(x), uialert(obj.Fig,'没有可播放的音频。','提示'); return; end
            try
                obj.stopPlayback(); obj.setBusy(true); cleanup=onCleanup(@()obj.finish()); %#ok<NASGU>
                w=audioDeviceWriter('SampleRate',obj.Fs);
                wc=onCleanup(@()release(w)); %#ok<NASGU>
                if ~strcmp(obj.OutputDevice.Value,'默认'), w.Device=obj.OutputDevice.Value; end
                h=1024; x=[x;zeros(mod(-numel(x),h),1)];
                for k=1:h:numel(x)
                    if obj.StopRequested, break; end
                    w(x(k:k+h-1)); drawnow limitrate;
                end
                obj.Status.Text='播放结束。';
            catch e, obj.fail(e); end
        end
        function saveAudio(obj,processed)
            x=obj.Raw; name='original.wav';
            if processed, x=obj.Output; name='voice_changed.wav'; end
            if isempty(x), uialert(obj.Fig,'没有可导出的音频。','提示'); return; end
            [f,p]=uiputfile('*.wav','导出 16 kHz / 16-bit WAV',name);
            if isequal(f,0), return; end
            try
                audiowrite(fullfile(p,f),max(-1,min(1,x)),obj.Fs,'BitsPerSample',16);
                obj.Status.Text=['已导出：' fullfile(p,f)];
            catch e, obj.fail(e); end
        end
        function drawAudio(obj,x,y)
            ix=unique(round(linspace(1,numel(x),min(2500,numel(x)))));
            iy=unique(round(linspace(1,numel(y),min(2500,numel(y)))));
            obj.WaveIn.XData=(ix-1)/obj.Fs; obj.WaveIn.YData=x(ix);
            obj.WaveOut.XData=(iy-1)/obj.Fs; obj.WaveOut.YData=y(iy);
            obj.SpecIn.YData=VoiceProcessor.spectrum(x); obj.SpecOut.YData=VoiceProcessor.spectrum(y);
        end
        function setBusy(obj,value)
            obj.Busy=value; obj.StopRequested=false; obj.stopPlayback();
            state='on'; if value, state='off'; end
            for k=1:numel(obj.Controls), obj.Controls{k}.Enable=state; end
        end
        function stopPlayback(obj)
            if ~isempty(obj.Player), stop(obj.Player); obj.Player=[]; end
        end
        function stop(obj)
            obj.StopRequested=true; obj.stopPlayback();
        end
        function finish(obj)
            obj.Busy=false;
            if obj.Closing
                if isvalid(obj.Fig), delete(obj.Fig); end
            elseif isvalid(obj.Fig)
                obj.setBusy(false);
            end
        end
        function close(obj)
            obj.Closing=true; obj.stop();
            if ~obj.Busy, delete(obj.Fig); end
        end
        function fail(obj,e)
            if obj.Closing, return; end
            obj.Status.Text=['提示：' e.message];
            if ~strcmp(e.identifier,'VoiceLab:Cancelled')
                uialert(obj.Fig,sprintf('%s\n\n设备问题请检查 Windows 麦克风权限、设备占用和 16 kHz 支持。',e.message),'运行提示');
            end
        end
    end
    methods(Static)
        function db=level(x)
            db=20*log10(max(sqrt(mean(x.^2)),1e-6));
        end
    end
end
