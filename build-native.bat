@echo off
setlocal
cd /d "%~dp0"

echo KVALTIK TSC HUB - NATIVE BUILD
echo.

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo C++ Build Tools nejsou nainstalovane.
  echo Nemusis je instalovat - EXE lze sestavit pres GitHub Actions.
  pause
  exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
  echo C++ Build Tools nejsou nainstalovane.
  pause
  exit /b 1
)

call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if not exist build mkdir build

cl /nologo /std:c++17 /EHsc /O2 /MT /DUNICODE /D_UNICODE /Fe:"build\KvaltikTSCHub.exe" main.cpp /link /SUBSYSTEM:WINDOWS user32.lib comctl32.lib shell32.lib

if errorlevel 1 (
  echo Build selhal.
  pause
  exit /b 1
)

echo Hotovo: build\KvaltikTSCHub.exe
pause
