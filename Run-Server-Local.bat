@echo off
rem Same as Run-Server.bat, but only this computer can connect (127.0.0.1).
call "%~dp0Run-Server.bat" --host 127.0.0.1 %*
