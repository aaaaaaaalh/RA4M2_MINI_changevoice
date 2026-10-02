$ErrorActionPreference = 'Stop'
$voicePython = Join-Path $env:USERPROFILE 'VoiceLabAI\.venv\Scripts\python.exe'
if (-not (Test-Path $voicePython)) { throw "Python environment not found: $voicePython" }
& $voicePython -m pip install 'pyserial==3.5'
if ($LASTEXITCODE -ne 0) { throw 'pyserial installation failed' }
Write-Host 'Board serial support is ready. Existing AI dependencies were not upgraded.'
