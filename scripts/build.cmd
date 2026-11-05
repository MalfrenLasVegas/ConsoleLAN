@echo off
setlocal
set "VSLANG=1033"
cd /d "%~dp0.."
if not defined Qt6_ROOT set "Qt6_ROOT=D:\Qt\6.12.0\msvc2022_64"
if defined VSCMD_VER goto compile
for /f "usebackq tokens=*" %%i in (`"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "CONSOLELAN_VS=%%i"
if not defined CONSOLELAN_VS exit /b 1
call "%CONSOLELAN_VS%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
:compile
set "VSLANG=1033"
cmake -S . -B build-x64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%Qt6_ROOT%"
if not "%errorlevel%"=="0" exit /b 1
cmake --build build-x64 --parallel
if not "%errorlevel%"=="0" exit /b 1
set "PATH=%Qt6_ROOT%\bin;%PATH%"
ctest --test-dir build-x64 --output-on-failure
if not "%errorlevel%"=="0" exit /b 1
exit /b 0
