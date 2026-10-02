% Close the previous VoiceLab windows before running this script.
% This entry selects this extracted copy, avoiding older copies on the path.
clear classes
voicelabRoot=fileparts(mfilename('fullpath'));
voicelabMatlab=fullfile(voicelabRoot,'matlab');
assert(isfolder(voicelabMatlab),'请完整解压程序包，再运行启动脚本。');
addpath(voicelabMatlab,'-begin');
cd(voicelabMatlab);
app=VoiceLab;
