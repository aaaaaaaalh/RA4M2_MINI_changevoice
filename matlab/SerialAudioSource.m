classdef SerialAudioSource < handle
    properties
        Port=[]
        Parser
        LastPacketTime
        DCState=0
        DCStarted=false
        LastADCMean=NaN
        LastADCMin=NaN
        LastADCMax=NaN
        ClipSamples=0
        ReceivedSamples=0
        ADCMin=4095
        ADCMax=0
        ADCSum=0
        PeakToPeakMax=0
    end
    methods
        function obj=SerialAudioSource(port)
            obj.Parser=AudioPacketParser;
            obj.Port=serialport(port,921600,'Timeout',0.2,'DataBits',8,'Parity','none','StopBits',1,'FlowControl','none');
            flush(obj.Port,'input'); obj.LastPacketTime=tic;
        end
        function x=readFrame(obj,cancel,removeDC)
            if nargin<3, removeDC=true; end
            x=[];
            while ~cancel()
                n=obj.Port.NumBytesAvailable;
                if n>170000, error('VoiceLab:Backlog','串口接收积压超过约 5 秒，请停止后重新开始。'); end
                if n>0
                    before=obj.Parser.Packets;
                    obj.Parser.feed(read(obj.Port,n,'uint8'));
                    if obj.Parser.Packets>before, obj.LastPacketTime=tic; end
                end
                [x,synthetic]=obj.Parser.pop();
                if ~isempty(x)
                    if ~synthetic
                        adc=x*2048+2048;
                        obj.LastADCMean=mean(adc); obj.LastADCMin=min(adc); obj.LastADCMax=max(adc);
                        obj.ADCMin=min(obj.ADCMin,min(adc)); obj.ADCMax=max(obj.ADCMax,max(adc));
                        obj.ADCSum=obj.ADCSum+sum(adc); obj.ReceivedSamples=obj.ReceivedSamples+numel(adc);
                        obj.ClipSamples=obj.ClipSamples+nnz(adc<=4 | adc>=4091);
                        obj.PeakToPeakMax=max(obj.PeakToPeakMax,max(adc)-min(adc));
                        if removeDC
                            if ~obj.DCStarted
                                obj.DCState=-x(1); obj.DCStarted=true;
                            end
                            [x,obj.DCState]=filter([1 -1],[1 -0.995],x,obj.DCState);
                        end
                    else
                        % Zero insertion keeps the time axis; reset DC state after the gap.
                        obj.DCStarted=false;
                    end
                    return
                end
                if toc(obj.LastPacketTime)>5
                    error('VoiceLab:Timeout','5 秒内没有完整音频包。请检查 COM、固件、运行状态与 921600 波特率。');
                end
                drawnow limitrate; pause(0.001);
            end
        end
        function s=summary(obj)
            q=size(obj.Parser.Queue,2)*0.016;
            s=sprintf('包 %d | 丢包 %d | 坏同步 %d | 缓冲 %.2fs | ADC %.0f [%g,%g] | 削顶 %d', ...
                obj.Parser.Packets,obj.Parser.Lost,obj.Parser.Bad,q,obj.LastADCMean, ...
                obj.LastADCMin,obj.LastADCMax,obj.ClipSamples);
        end
        function release(obj)
            if ~isempty(obj.Port)
                delete(obj.Port); obj.Port=[];
            end
        end
        function delete(obj), obj.release(); end
    end
end
