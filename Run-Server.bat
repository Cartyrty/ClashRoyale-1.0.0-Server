@echo off
rem Runs the game server on port 9339 and the content update server on 9340, for this computer and others on the
rem network (Windows may ask to allow it through the firewall). Saved state is in server-data\.
rem The exe runs from a copy in bin\run, so Build-Server.bat can replace bin\ClashRoyaleServer.exe while it runs.
setlocal
cd /d "%~dp0"
if not exist bin\run mkdir bin\run
copy /y bin\ClashRoyaleServer.exe bin\run\ClashRoyaleServer.exe >nul
title Clash Royale v1 Server (port 9339)
set "HLE_QUIET=init,bridge,file,data,stdout,objc,macho,patch"
"%~dp0bin\run\ClashRoyaleServer.exe" --library "%~dp0game\library" --host 0.0.0.0 --port 9339 --data "%~dp0server-data" %*
pause
