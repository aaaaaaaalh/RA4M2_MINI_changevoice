classdef VoiceLabAI < handle
    % Seed-VC offline front end; uses the existing local Gradio server.
    properties
        Fig
        Status
        SourceLabel
        ReferenceLabel
        Python
        Steps
        CFG
        Buttons={}
        Raw=[]
        RawFs=48000
        Output=[]
        OutputFs=22050
        Reference=''
        Recorder=[]
        Player=[]
        Busy=false
        Closing=false
        Cancel=false
    end
    methods
        function obj=VoiceLabAI(raw,fs)
            obj.Fig=uifigure('Name','VoiceLab v0.3 · AI 参考音色变声', ...
                'Position',[160 140 760 570],'CloseRequestFcn',@(~,~)obj.close());
            g=uigridlayout(obj.Fig,[12 2]); g.ColumnWidth={180,'1x'};
            g.RowHeight={40,32,32,32,32,32,36,36,36,36,40,'1x'};
            t=uilabel(g,'Text','AI 参考音色变声 · Seed-VC','FontSize',22,'FontWeight','bold');
            t.Layout.Row=1; t.Layout.Column=[1 2];
            uilabel(g,'Text','Python（现有虚拟环境）');
            obj.Python=uieditfield(g,'text','Value',fullfile(getenv('USERPROFILE'),'VoiceLabAI','.venv','Scripts','python.exe'));
            obj.addButton(g,3,1,'导入原声',@()obj.importSource());
            obj.SourceLabel=uilabel(g,'Text','未录音 / 未导入','WordWrap','on'); obj.SourceLabel.Layout.Row=3; obj.SourceLabel.Layout.Column=2;
            obj.addButton(g,4,1,'选择参考音色 WAV / FLAC',@()obj.selectReference());
            obj.ReferenceLabel=uilabel(g,'Text','选择唐三等参考片段，建议 10～20 秒','WordWrap','on'); obj.ReferenceLabel.Layout.Row=4; obj.ReferenceLabel.Layout.Column=2;
            uilabel(g,'Text','扩散步数'); obj.Steps=uieditfield(g,'numeric','Limits',[1 200],'RoundFractionalValues','on','Value',25);
            uilabel(g,'Text','CFG'); obj.CFG=uieditfield(g,'numeric','Limits',[0 1],'Value',0.7);
            obj.addButton(g,7,1,'开始麦克风录音',@()obj.record());
            b=uibutton(g,'Text','停止录音 / 取消转换 / 停止播放','ButtonPushedFcn',@(~,~)obj.stop()); b.Layout.Row=7; b.Layout.Column=2;
            obj.addButton(g,8,1,'执行 AI 变声',@()obj.convert());
            t=uilabel(g,'Text','长度调整固定 1.0；保留原语速和总时长'); t.Layout.Row=8; t.Layout.Column=2;
            obj.addButton(g,9,1,'播放原声',@()obj.play(false));
            obj.addButton(g,9,2,'播放变声结果',@()obj.play(true));
            obj.addButton(g,10,1,'导出原声 WAV',@()obj.export(false));
            obj.addButton(g,10,2,'导出变声 WAV',@()obj.export(true));
            t=uilabel(g,'Text','先保持 Seed-VC 后台运行（127.0.0.1:7860）；无需在网页上传音频。','WordWrap','on'); t.Layout.Row=11; t.Layout.Column=[1 2];
            obj.Status=uilabel(g,'Text','就绪 · 使用 Windows 默认麦克风和播放设备。','WordWrap','on'); obj.Status.Layout.Row=12; obj.Status.Layout.Column=[1 2];
            obj.Buttons=[obj.Buttons,{obj.Python,obj.Steps,obj.CFG}];
            if nargin>=2 && ~isempty(raw)
                obj.Raw=raw; obj.RawFs=fs; obj.describeSource('主窗口录音');
            end
        end
        function addButton(obj,g,row,col,label,callback)
            b=uibutton(g,'Text',label,'ButtonPushedFcn',@(~,~)callback());
            b.Layout.Row=row; b.Layout.Column=col; obj.Buttons{end+1}=b;
        end
        function describeSource(obj,name)
            obj.SourceLabel.Text=sprintf('%s · %.2f 秒 / %d Hz',name,numel(obj.Raw)/obj.RawFs,obj.RawFs);
        end
        function importSource(obj)
            [f,p]=uigetfile({'*.wav;*.flac;*.mp3;*.m4a','音频文件'}); if isequal(f,0), return; end
            try
                info=audioinfo(fullfile(p,f)); assert(info.Duration<=300,'最多导入 5 分钟。');
                [x,fs]=audioread(fullfile(p,f)); x=mean(x,2);
                assert(~isempty(x)&&all(isfinite(x)),'音频为空或无效。');
                obj.stopPlayer(); obj.Raw=x; obj.RawFs=fs; obj.Output=[]; obj.describeSource(f);
            catch e, obj.fail(e); end
        end
        function selectReference(obj)
            [f,p]=uigetfile({'*.wav;*.flac','参考音色'}); if isequal(f,0), return; end
            try
                path=fullfile(p,f); info=audioinfo(path); assert(info.Duration>=1,'参考音频太短。');
                obj.Reference=path; obj.ReferenceLabel.Text=sprintf('%s · %.1f 秒',f,info.Duration);
                if info.Duration>25, obj.Status.Text='参考音频超过 25 秒：模型只使用前 25 秒。'; end
            catch e, obj.fail(e); end
        end
        function record(obj)
            try
                obj.stopPlayer(); obj.Recorder=audiorecorder(48000,16,1);
                obj.Recorder.StopFcn=@(~,~)obj.recordFinished();
                obj.setBusy(true); record(obj.Recorder,300);
                obj.Status.Text='录音中；点击停止结束，最长 5 分钟。';
            catch e, obj.setBusy(false); obj.fail(e); end
        end
        function recordFinished(obj)
            if obj.Closing, return; end
            try
                x=getaudiodata(obj.Recorder);
                if ~isempty(x), obj.Raw=x; obj.RawFs=48000; obj.Output=[]; obj.describeSource('麦克风录音'); end
                obj.setBusy(false); obj.Status.Text='录音结束，可导出原声或执行 AI 变声。';
            catch e, obj.setBusy(false); obj.fail(e); end
        end
        function convert(obj)
            proc=[]; work='';
            try
                assert(ispc,'此桥接版针对 Windows MATLAB。');
                assert(~isempty(obj.Raw),'请先录音或导入原声。');
                assert(isfile(obj.Reference),'请先选择参考音色。');
                assert(isfile(obj.Python.Value),'找不到 Python，请检查虚拟环境路径。');
                obj.stopPlayer(); obj.Cancel=false; obj.setBusy(true);
                work=tempname; mkdir(work);
                cleanup=onCleanup(@()obj.finishWork(work)); %#ok<NASGU>
                source=fullfile(work,'source.wav'); audiowrite(source,obj.Raw,obj.RawFs,'BitsPerSample',24);
                cfg=struct('source',source,'reference',obj.Reference,'steps',obj.Steps.Value,'cfg',obj.CFG.Value);
                request=fullfile(work,'request.json');
                fid=fopen(request,'w','n','UTF-8'); assert(fid>=0,'无法创建任务文件。');
                fprintf(fid,'%s',jsonencode(cfg)); fclose(fid);
                bridge=fullfile(fileparts(fileparts(mfilename('fullpath'))),'python','seed_vc_bridge.py');
                assert(isfile(bridge),'缺少 python/seed_vc_bridge.py，请完整解压。');
                proc=System.Diagnostics.Process;
                proc.StartInfo.FileName=obj.Python.Value;
                proc.StartInfo.Arguments=sprintf('"%s" "%s"',bridge,request);
                proc.StartInfo.UseShellExecute=false; proc.StartInfo.CreateNoWindow=true;
                proc.StartInfo.EnvironmentVariables.Item('PYTHONUTF8')='1';
                assert(proc.Start(),'Python 启动失败。');
                obj.Status.Text='AI 转换中…首次连接可能稍慢，可点击取消。';
                started=tic; cancelAt=inf;
                while ~proc.HasExited
                    drawnow; pause(0.15);
                    if obj.Cancel && isinf(cancelAt)
                        fid=fopen(fullfile(work,'cancel'),'w'); if fid>=0, fclose(fid); end
                        cancelAt=toc(started);
                    end
                    if toc(started)>1860 || toc(started)-cancelAt>10
                        proc.Kill(); proc.WaitForExit(5000); break;
                    end
                end
                if obj.Cancel, error('VoiceLab:Cancelled','已取消等待；后台当前计算可能仍需片刻结束。'); end
                resultPath=fullfile(work,'result.json');
                assert(isfile(resultPath),'后台未返回结果，请检查 Python 路径、依赖或 Seed-VC 后台。');
                result=jsondecode(fileread(resultPath));
                if ~result.ok, error('VoiceLab:AI', '%s',result.error); end
                [y,fs]=audioread(result.output); obj.Output=y; obj.OutputFs=fs;
                obj.Status.Text=sprintf('AI 变声完成 · %.2f 秒 / %d Hz，可试听或导出。',numel(y)/fs,fs);
            catch e
                if ~isempty(proc)
                    try, if ~proc.HasExited, proc.Kill(); proc.WaitForExit(5000); end, catch, end
                end
                obj.fail(e);
            end
            if ~isempty(proc), proc.Dispose(); end
        end
        function finishWork(obj,work)
            if isfolder(work), try, rmdir(work,'s'); catch, end, end
            if obj.Closing
                if isvalid(obj.Fig), delete(obj.Fig); end
            else, obj.setBusy(false); end
        end
        function play(obj,converted)
            x=obj.Raw; fs=obj.RawFs; if converted, x=obj.Output; fs=obj.OutputFs; end
            if isempty(x), obj.Status.Text='没有可播放音频。'; return; end
            obj.stopPlayer(); obj.Player=audioplayer(x,fs); play(obj.Player);
        end
        function export(obj,converted)
            x=obj.Raw; fs=obj.RawFs; name='original.wav';
            if converted, x=obj.Output; fs=obj.OutputFs; name='ai_voice.wav'; end
            if isempty(x), obj.Status.Text='没有可导出音频。'; return; end
            [f,p]=uiputfile('*.wav','导出 WAV',name); if isequal(f,0), return; end
            try, audiowrite(fullfile(p,f),x,fs,'BitsPerSample',24); obj.Status.Text=['已导出：' fullfile(p,f)]; catch e, obj.fail(e); end
        end
        function setBusy(obj,value)
            obj.Busy=value; state='on'; if value, state='off'; end
            for k=1:numel(obj.Buttons), obj.Buttons{k}.Enable=state; end
        end
        function stopPlayer(obj)
            if ~isempty(obj.Player), stop(obj.Player); obj.Player=[]; end
        end
        function stop(obj)
            obj.Cancel=true; obj.stopPlayer();
            if ~isempty(obj.Recorder)&&isrecording(obj.Recorder), stop(obj.Recorder); end
        end
        function close(obj)
            recording=~isempty(obj.Recorder)&&isrecording(obj.Recorder);
            obj.Closing=true; obj.stop();
            if ~obj.Busy||recording, delete(obj.Fig); end
        end
        function fail(obj,e)
            if obj.Closing||~isvalid(obj.Fig), return; end
            obj.Status.Text=['提示：' e.message];
            if ~strcmp(e.identifier,'VoiceLab:Cancelled'), uialert(obj.Fig,e.message,'AI 变声提示'); end
        end
    end
end
