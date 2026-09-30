classdef VoiceProcessor < handle
    % Stateful 16 kHz / 256 sample processor. No audio-device dependencies.
    properties
        Fs = 16000
        H = 256
        P
        NoisePower
        Prev = zeros(256,1)
        Tail = zeros(256,1)
        Window
        Pitcher = []
        Zhp
        Zlp
        Ztone
        BHP
        AHP
        BLP
        ALP
        BT
        AT
        Count = 0
        Limited = 0
    end
    methods
        function obj = VoiceProcessor(p,noise,streaming)
            obj.P = p; obj.NoisePower = noise;
            obj.Window = sqrt(hann(512,'periodic'));
            [obj.BHP,obj.AHP] = butter(2,p.Highpass/(obj.Fs/2),'high');
            [obj.BLP,obj.ALP] = butter(2,p.Lowpass/(obj.Fs/2),'low');
            [obj.BT,obj.AT] = butter(1,1200/(obj.Fs/2),'low');
            obj.Zhp = zeros(2,1); obj.Zlp = zeros(2,1); obj.Ztone = 0;
            if streaming && abs(p.Semitones)>1e-8
                obj.Pitcher = audiopluginexample.PitchShifter( ...
                    'PitchShift',p.Semitones,'Overlap',0.2);
                setSampleRate(obj.Pitcher,obj.Fs);
            end
        end
        function y = denoise(obj,x)
            assert(numel(x)==obj.H,'Expected 256 samples.');
            if ~obj.P.Denoise
                y=x; return
            end
            f=[obj.Prev;x]; obj.Prev=x;
            X=fft(f.*obj.Window); power=abs(X).^2;
            gain=sqrt(max(1-obj.P.Alpha*obj.NoisePower./max(power,1e-16),obj.P.Beta));
            z=real(ifft(X.*gain)).*obj.Window;
            y=obj.Tail+z(1:obj.H); obj.Tail=z(obj.H+1:end);
        end
        function y = step(obj,x)
            y=obj.denoise(x);
            if ~isempty(obj.Pitcher), y=obj.Pitcher(y); end
            y=obj.color(y);
        end
        function y = color(obj,y)
            [y,obj.Zhp]=filter(obj.BHP,obj.AHP,y,obj.Zhp);
            [y,obj.Zlp]=filter(obj.BLP,obj.ALP,y,obj.Zlp);
            [lo,obj.Ztone]=filter(obj.BT,obj.AT,y,obj.Ztone);
            b=obj.P.Brightness;
            y=10^(-b/40)*lo+10^(b/40)*(y-lo);
            t=(obj.Count+(0:numel(y)-1)')/obj.Fs;
            if obj.P.Robot, y=y.*cos(2*pi*obj.P.Carrier*t); end
            if obj.P.Old, y=y.*(1+0.07*sin(2*pi*5*t)); end
            obj.Count=obj.Count+numel(y);
            y=y*10^(obj.P.Gain/20);
            obj.Limited=obj.Limited+nnz(abs(y)>0.95);
            % Continuous soft knee, exactly unity below the knee.
            k=abs(y)>0.95;
            y(k)=sign(y(k)).*(0.95+0.05*tanh((abs(y(k))-0.95)/0.05));
        end
        function delete(obj)
            if ~isempty(obj.Pitcher), release(obj.Pitcher); end
        end
    end
    methods(Static)
        function p = defaults()
            p=struct('Semitones',0,'Brightness',0,'Carrier',130,'Gain',0, ...
                'Denoise',false,'Alpha',1.5,'Beta',0.04,'Highpass',60, ...
                'Lowpass',7400,'Robot',false,'Old',false,'PreserveFormants',false);
        end
        function noise = learn(x)
            x=x(:); h=256; w=sqrt(hann(512,'periodic'));
            assert(numel(x)>=512,'Noise recording must contain at least 512 samples.');
            noise=zeros(512,1); count=0;
            for k=1:h:numel(x)-511
                noise=noise+abs(fft(x(k:k+511).*w)).^2; count=count+1;
            end
            noise=noise/count;
        end
        function bars = spectrum(x)
            x=x(:); x=[zeros(max(0,512-numel(x)),1);x]; x=x(end-511:end);
            w=hann(512,'periodic'); a=abs(fft(x.*w))/sum(w);
            a=a(2:257); a(1:255)=2*a(1:255);
            bars=20*log10(max(max(reshape(a,8,32),[],1),1e-5));
        end
        function y = offline(x,p,noise,progress)
            if nargin<4, progress=@(~)true; end
            x=double(x(:)); n=numel(x); h=256;
            engine=VoiceProcessor(p,noise,false); cleanup=onCleanup(@()delete(engine)); %#ok<NASGU>
            if p.Denoise
                pad=ceil(n/h)*h; z=[x;zeros(pad-n+h,1)]; d=zeros(size(z));
                for k=1:h:numel(z)
                    d(k:k+h-1)=engine.denoise(z(k:k+h-1));
                    if mod(k-1,256*64)==0 && ~progress(0.35*k/numel(z)), error('VoiceLab:Cancelled','处理已取消'); end
                end
                x=d(h+1:h+n); % compensate the exact OLA delay, retain final samples
            end
            if ~progress(0.4), error('VoiceLab:Cancelled','处理已取消'); end
            if abs(p.Semitones)>1e-8
                % Pad very short clips for the phase vocoder, then restore length.
                padded=[x;zeros(max(0,4096-n),1)];
                y=shiftPitch(padded,p.Semitones,'LockPhase',true, ...
                    'PreserveFormants',p.PreserveFormants);
                y=[y;zeros(max(0,n-numel(y)),1)]; y=y(1:n);
            else
                y=x;
            end
            if ~progress(0.8), error('VoiceLab:Cancelled','处理已取消'); end
            y=engine.color(y);
            if ~progress(1), error('VoiceLab:Cancelled','处理已取消'); end
        end
    end
end
