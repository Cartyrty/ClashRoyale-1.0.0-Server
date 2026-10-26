@echo off
rem Makes a new content patch from the master assets: game\res (the files the server runs on), or the folder given as
rem the first argument. Clients with other content download the changed files at their next login. Only changes
rem since the last patch make a new one. Restart the server afterwards: it loads the newest patch when it starts.
setlocal
cd /d "%~dp0"
set "MASTER=%~1"
if not defined MASTER set "MASTER=%~dp0game\res"
"%~dp0bin\ClashRoyaleServer.exe" --library "%~dp0game\library" --build-content "%MASTER%" --data "%~dp0server-data"
pause
