@echo off
rem Recompiles ClashRoyaleServer from source\ into bin\ClashRoyaleServer.exe.
rem Needs Visual Studio 2019/2022 (or Build Tools) with "Desktop development with C++". The first build takes a while.
setlocal
cd /d "%~dp0"

set "CMAKE="
for %%c in (cmake.exe) do set "CMAKE=%%~$PATH:c"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not defined CMAKE if exist "%VSWHERE%" for /f "usebackq delims=" %%c in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`) do set "CMAKE=%%c"
if not defined CMAKE goto nocmake

echo Using "%CMAKE%"
"%CMAKE%" -S source -B source\build -A x64 || goto fail
"%CMAKE%" --build source\build --config Release --parallel --target ClashRoyaleServer || goto fail
echo Restart Run-Server.bat afterwards to use the new build.
copy /y "source\build\Release\ClashRoyaleServer.exe" "bin\ClashRoyaleServer.exe" >nul || goto fail
echo.
echo Done: bin\ClashRoyaleServer.exe
pause
exit /b 0

:nocmake
echo CMake and the Visual Studio C++ compiler were not found.
echo Install Visual Studio 2019 or 2022 (Community or Build Tools) with the "Desktop development with C++" workload.
pause
exit /b 1
:fail
echo.
echo The build failed (see the errors above).
echo If they mention paths that cannot be found or created (for example FTK1011), the folder is too deep for Windows'
echo path limit: move the package somewhere short (for example C:\ClashRoyale) and run this again.
pause
exit /b 1
