@echo off
setlocal EnableExtensions
set "REPO=%~dp0..\.."
if defined UNDERDOG_FEATURE_PROBE_OUT (
  set "OUT=%UNDERDOG_FEATURE_PROBE_OUT%"
) else (
  set "OUT=%TEMP%\underdog_feature_probe_v01"
)
if not exist "%OUT%" mkdir "%OUT%"
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
cl.exe /nologo /std:c++17 /EHsc /W4 /O2 /Fo"%OUT%\dx12_feature_probe_v01.obj" /Fe:"%OUT%\dx12_feature_probe_v01.exe" "%REPO%\tools\diagnostics\dx12_feature_probe_v01.cpp" /link d3d12.lib dxgi.lib
exit /b %errorlevel%
