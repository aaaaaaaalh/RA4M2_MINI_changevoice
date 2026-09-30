function selftest_serial
% Pure protocol tests; no serial hardware needed.
a=packet(65535,-32768); b=packet(0,32752); c=packet(2,0);
p=AudioPacketParser;
stream=[uint8([9 8 170]),a,b,c];
for k=1:37:numel(stream), p.feed(stream(k:min(k+36,end))); end
assert(p.Packets==3 && p.Lost==1);
[x,s]=p.pop(); assert(~s && all(x==-1));
[x,s]=p.pop(); assert(~s && all(x==32752/32768));
[x,s]=p.pop(); assert(s && all(x==0));
[x,s]=p.pop(); assert(~s && all(x==0)); assert(isempty(p.pop()));
% Partial header must not produce audio; arbitrary payload AA55 is legal.
p=AudioPacketParser; p.feed(a(1:5)); assert(isempty(p.pop()));
p.feed([a(6:end),b]); assert(p.Packets==2);
% Lost byte: reject corrupt frame boundary and find two consistent headers.
p=AudioPacketParser;
p.feed([packet(10,0),packet(11,0)]); p.pop();p.pop();
damaged=packet(12,0); damaged(100)=[];
p.feed([uint8(77),damaged,packet(13,0),packet(14,0)]);
assert(p.Packets==4 && p.Lost==1 && p.Bad==1);
[x,s]=p.pop(); assert(s && all(x==0));
% Reset/backwards sequence is an explicit failure, not 65535 silent frames.
p=AudioPacketParser; p.feed([packet(100,0),packet(101,0)]);
failed=false;
try, p.feed(packet(0,0)); catch e, failed=strcmp(e.identifier,'VoiceLab:Sequence'); end
assert(failed);
fprintf('Serial parser tests passed.\n');
end
function bytes=packet(seq,sample)
u=mod(double(sample),65536);
bytes=uint8([170,85,0,2,mod(seq,256),floor(seq/256),repmat([mod(u,256),floor(u/256)],1,256)]);
end
