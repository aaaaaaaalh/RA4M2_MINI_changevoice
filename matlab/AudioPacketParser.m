classdef AudioPacketParser < handle
    % AA55 + uint16 length (512) + uint16 seq + 256 little-endian PCM16.
    properties
        Buffer = uint8([])
        Locked = false
        LastSequence = []
        Packets = 0
        Lost = 0
        Bad = 0
        SkippedBytes = 0
        Queue = zeros(256,0)
        Synthetic = false(1,0)
    end
    methods
        function feed(obj,bytes)
            obj.Buffer=[obj.Buffer,reshape(uint8(bytes),1,[])];
            while numel(obj.Buffer)>=6
                b=obj.Buffer;
                if b(1)~=170 || b(2)~=85 || b(3)~=0 || b(4)~=2
                    obj.Buffer(1)=[]; obj.SkippedBytes=obj.SkippedBytes+1;
                    if obj.Locked, obj.Bad=obj.Bad+1; end
                    obj.Locked=false; continue
                end
                if numel(b)<518, return; end
                seq=double(b(5))+256*double(b(6));
                if ~obj.Locked
                    % Require a second valid header at the expected offset.
                    if numel(b)<524, return; end
                    ns=double(b(523))+256*double(b(524));
                    delta=mod(ns-seq,65536);
                    if ~isequal(b(519:522),uint8([170 85 0 2])) || delta<1 || delta>257
                        obj.Buffer(1)=[]; obj.SkippedBytes=obj.SkippedBytes+1; continue
                    end
                    obj.Locked=true;
                end
                if ~isempty(obj.LastSequence)
                    delta=mod(seq-obj.LastSequence,65536);
                    if delta==0
                        obj.Buffer(1:518)=[]; obj.Bad=obj.Bad+1; continue
                    elseif delta>257
                        error('VoiceLab:Sequence','序号跳变过大，可能板卡复位。请停止后重新开始。');
                    end
                    lost=delta-1; obj.Lost=obj.Lost+lost;
                    if lost>0
                        obj.Queue=[obj.Queue,zeros(256,lost)];
                        obj.Synthetic=[obj.Synthetic,true(1,lost)];
                    end
                end
                u=double(b(7:2:518))+256*double(b(8:2:518));
                u(u>=32768)=u(u>=32768)-65536;
                obj.Queue(:,end+1)=u(:)/32768;
                obj.Synthetic(end+1)=false;
                obj.LastSequence=seq; obj.Packets=obj.Packets+1;
                obj.Buffer(1:518)=[];
                if size(obj.Queue,2)>320
                    error('VoiceLab:Backlog','串口积压超过约 5 秒，请停止并降低界面刷新负载。');
                end
            end
        end
        function [x,synthetic]=pop(obj)
            x=[]; synthetic=false;
            if ~isempty(obj.Queue)
                x=obj.Queue(:,1); obj.Queue(:,1)=[];
                synthetic=obj.Synthetic(1); obj.Synthetic(1)=[];
            end
        end
    end
end
