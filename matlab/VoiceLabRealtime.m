classdef VoiceLabRealtime < handle
    % MATLAB control surface; audio runs in the verified Seed-VC Python engine.
    properties
        Fig
        Python
        Repo
        Host
        Source
        Com
        Gain
        Input
        Output
        RefText
        Status
        Refresh
        Browse
        Start
        Stop
        Devices=[]
        Reference=''
        Proc=[]
        Poller=[]
        Work=''
        Action=''
        Stopping=false
        StopClock
        Closing=false
        LastStatus=''
    end
    methods
        function obj=VoiceLabRealtime()
            obj.Fig=uifigure('Name','VoiceLab · 实时 AI 变声 v0.4.0', ...
                'Position',[160 80 820 690],'CloseRequestFcn',@(~,~)obj.close());
            g=uigridlayout(obj.Fig,[15 2]); g.ColumnWidth={170,'1x'};
            g.RowHeight={42,32,32,32,32,32,32,32,32,32,40,44,40,'1x',36};
            t=uilabel(g,'Text','实时 AI 变声 · 参考音色','FontSize',22,'FontWeight','bold'); t.Layout.Column=[1 2];
            uilabel(g,'Text','Python 虚拟环境');
            obj.Python=uieditfield(g,'Value',fullfile(getenv('USERPROFILE'),'VoiceLabAI','.venv','Scripts','python.exe'));
            uilabel(g,'Text','Seed-VC 文件夹');
            obj.Repo=uieditfield(g,'Value',fullfile(getenv('USERPROFILE'),'VoiceLabAI','seed-vc'));
            obj.Browse=uibutton(g,'Text','选择参考音色','ButtonPushedFcn',@(~,~)obj.choose());
            obj.RefText=uilabel(g,'Text','选择已验证的唐三 WAV / FLAC','WordWrap','on');
            uilabel(g,'Text','输入来源'); obj.Source=uidropdown(g,'Items',{'电脑麦克风','RA4M2 板卡'},'ValueChangedFcn',@(~,~)obj.sourceChanged());
            uilabel(g,'Text','板卡 COM 口'); obj.Com=uieditfield(g,'Value','COM3','Enable','off');
            uilabel(g,'Text','板卡输入增益（dB）'); obj.Gain=uieditfield(g,'numeric','Value',0,'Limits',[-12 24],'Enable','off');
            uilabel(g,'Text','设备驱动类型'); obj.Host=uidropdown(g,'Items',{'请先刷新设备'},'ValueChangedFcn',@(~,~)obj.updateDevices());
            uilabel(g,'Text','输入麦克风'); obj.Input=uidropdown(g,'Items',{'请先刷新设备'});
            uilabel(g,'Text','输出耳机'); obj.Output=uidropdown(g,'Items',{'请先刷新设备'});
            t=uilabel(g,'Text','稳定配置：块时间 0.70 秒 · 6 步 · CFG 0.7 · 参考长度 3 秒','WordWrap','on'); t.Layout.Column=[1 2];
            obj.Start=uibutton(g,'Text','开始实时 AI 变声','ButtonPushedFcn',@(~,~)obj.start());
            obj.Stop=uibutton(g,'Text','停止','Enable','off','ButtonPushedFcn',@(~,~)obj.stop());
            t=uilabel(g,'Text','请戴耳机；关闭其他实时窗口及离线后台。板卡模式须先释放被其他程序占用的串口。','WordWrap','on'); t.Layout.Column=[1 2];
            obj.Status=uilabel(g,'Text','就绪。先刷新设备，再选择麦克风和耳机。','WordWrap','on'); obj.Status.Layout.Column=[1 2];
            obj.Refresh=uibutton(g,'Text','刷新设备','ButtonPushedFcn',@(~,~)obj.launch('devices'));
            uibutton(g,'Text','打开本次后台日志','ButtonPushedFcn',@(~,~)obj.log());
            obj.Poller=timer('ExecutionMode','fixedSpacing','Period',0.5,'BusyMode','drop','TimerFcn',@(~,~)obj.poll());
        end
        function sourceChanged(obj)
            board=strcmp(obj.Source.Value,'RA4M2 板卡');
            obj.Input.Enable='on'; obj.Com.Enable='off'; obj.Gain.Enable='off';
            if board, obj.Input.Enable='off'; obj.Com.Enable='on'; obj.Gain.Enable='on'; end
        end
        function choose(obj)
            [f,p]=uigetfile({'*.wav;*.flac','参考音色'}); if isequal(f,0), return; end
            obj.Reference=fullfile(p,f); obj.RefText.Text=f;
        end
        function updateDevices(obj)
            if isempty(obj.Devices), return; end
            d=obj.Devices; host=obj.Host.Value;
            ins=d(strcmp({d.host},host)&[d.inputs]>0); outs=d(strcmp({d.host},host)&[d.outputs]>0);
            obj.fill(obj.Input,ins); obj.fill(obj.Output,outs);
        end
        function fill(~,control,items)
            if isempty(items)
                control.ItemsData=[]; control.Items={'无可用设备'}; return;
            end
            labels=arrayfun(@(d)sprintf('[%d] %s',d.id,d.name),items,'UniformOutput',false);
            control.ItemsData=[]; control.Items=labels; control.ItemsData=[items.id]; control.Value=items(1).id;
        end
        function start(obj)
            if ~isfile(obj.Reference), uialert(obj.Fig,'请先选择参考音色文件。','提示'); return; end
            if isempty(obj.Devices)||~isnumeric(obj.Output.Value)|| ...
                    (strcmp(obj.Source.Value,'电脑麦克风')&&~isnumeric(obj.Input.Value))
                uialert(obj.Fig,'请先刷新并选择有效输入输出设备。','提示'); return;
            end
            obj.launch('run');
        end
        function launch(obj,action)
            if ~isempty(obj.Proc), return; end
            try
                assert(ispc,'本版实时桥接用于 Windows。');
                assert(isfile(obj.Python.Value),'找不到 Python 虚拟环境。');
                assert(isfile(fullfile(obj.Repo.Value,'real-time-gui.py')),'找不到 real-time-gui.py。');
                obj.Work=tempname; mkdir(obj.Work); obj.LastStatus='';
                cfg=struct('action',action,'repo',obj.Repo.Value);
                if strcmp(action,'run')
                    cfg.reference=obj.Reference;
                    cfg.source_kind='microphone';
                    if strcmp(obj.Source.Value,'RA4M2 板卡')
                        cfg.source_kind='board'; cfg.com_port=upper(strtrim(obj.Com.Value));
                        cfg.board_gain_db=obj.Gain.Value;
                    else
                        cfg.input=obj.Devices(find([obj.Devices.id]==obj.Input.Value,1));
                    end
                    cfg.output=obj.Devices(find([obj.Devices.id]==obj.Output.Value,1));
                end
                request=fullfile(obj.Work,'request.json');
                fid=fopen(request,'w','n','UTF-8'); assert(fid>=0,'无法写入任务文件。');
                fprintf(fid,'%s',jsonencode(cfg)); fclose(fid);
                script=fullfile(fileparts(fileparts(mfilename('fullpath'))),'python','seed_vc_realtime.py');
                assert(isfile(script),'缺少 python/seed_vc_realtime.py。');
                obj.Proc=System.Diagnostics.Process;
                info=System.Diagnostics.ProcessStartInfo;
                info.FileName=obj.Python.Value;
                info.Arguments=sprintf('-X utf8 "%s" "%s"',script,request);
                info.UseShellExecute=false; info.CreateNoWindow=true;
                obj.Proc.StartInfo=info;
                assert(obj.Proc.Start(),'Python 启动失败。');
                obj.Action=action; obj.Stopping=false; obj.busy(true);
                obj.Status.Text='正在启动后台…'; start(obj.Poller);
            catch e
                obj.dispose(); obj.busy(false); obj.Status.Text=e.message; uialert(obj.Fig,e.message,'启动失败');
            end
        end
        function busy(obj,value)
            state='on'; if value, state='off'; end
            controls={obj.Python,obj.Repo,obj.Host,obj.Input,obj.Output,obj.Browse,obj.Refresh,obj.Start,obj.Source,obj.Com,obj.Gain};
            for k=1:numel(controls), controls{k}.Enable=state; end
            obj.Stop.Enable='off'; if value, obj.Stop.Enable='on'; else, obj.sourceChanged(); end
        end
        function poll(obj)
            if isempty(obj.Proc), return; end
            try
                path=fullfile(obj.Work,'status.json');
                if isfile(path)
                    % Atomic replacement may briefly conflict with Windows reads.
                    try, s=jsondecode(fileread(path)); catch, return; end
                    if ~strcmp(obj.LastStatus,s.state)&&strcmp(s.state,'devices')
                        obj.Devices=s.devices;
                        if ~isempty(obj.Devices)
                            hosts=unique({obj.Devices.host},'stable'); obj.Host.Items=hosts;
                            if any(strcmp(hosts,'MME')), obj.Host.Value='MME'; else, obj.Host.Value=hosts{1}; end
                            obj.updateDevices();
                        end
                    end
                    obj.LastStatus=s.state;
                    if ~obj.Stopping
                        obj.Status.Text=s.message;
                        if strcmp(s.state,'running')
                            if isempty(s.infer_ms), val='预热中'; else, val=sprintf('%d ms',s.infer_ms); end
                            obj.Status.Text=sprintf('运行中 · 每块处理耗时 %s（不是总延迟）\n设备缓冲异常累计 %d · 按当前配置保留较长监听延迟',val,s.xruns);
                            if isfield(s,'board_status')&&~isempty(s.board_status)
                                obj.Status.Text=sprintf('%s\n%s',obj.Status.Text,s.board_status);
                            end
                        end
                    end
                end
                if obj.Stopping&&toc(obj.StopClock)>15&&~obj.Proc.HasExited
                    obj.Proc.Kill(); obj.Status.Text='已终止本次后台，释放音频设备。';
                end
                if obj.Proc.HasExited
                    code=obj.Proc.ExitCode; stop(obj.Poller); obj.dispose();
                    if obj.Closing, obj.destroy(); return; end
                    obj.busy(false);
                    if obj.Stopping, obj.Status.Text='实时 AI 已停止。';
                    elseif code~=0&&~strcmp(obj.LastStatus,'error')
                        obj.Status.Text='后台启动失败，请点击“打开本次后台日志”。';
                    elseif strcmp(obj.Action,'run')&&~strcmp(obj.LastStatus,'error')
                        obj.Status.Text='实时 AI 已停止。';
                    end
                end
            catch e
                obj.Status.Text=['后台监测错误：' e.message]; obj.stop();
            end
        end
        function stop(obj)
            if isempty(obj.Proc)||obj.Stopping, return; end
            fid=fopen(fullfile(obj.Work,'stop'),'w'); if fid>=0, fclose(fid); end
            obj.Stopping=true; obj.StopClock=tic;
            obj.Status.Text='正在停止…模型加载期间最长等待约 15 秒。';
        end
        function log(obj)
            path=fullfile(obj.Work,'backend.log');
            if isfile(path), edit(path); else, uialert(obj.Fig,'尚未生成后台日志。','提示'); end
        end
        function dispose(obj)
            if ~isempty(obj.Proc)
                try
                    if ~obj.Proc.HasExited, obj.Proc.Kill(); obj.Proc.WaitForExit(3000); end
                catch
                end
                obj.Proc.Dispose(); obj.Proc=[];
            end
        end
        function close(obj)
            obj.Closing=true;
            if isempty(obj.Proc), obj.destroy(); else, obj.stop(); end
        end
        function destroy(obj)
            if ~isempty(obj.Poller)&&isvalid(obj.Poller), stop(obj.Poller); delete(obj.Poller); end
            if isvalid(obj.Fig), delete(obj.Fig); end
        end
    end
end
