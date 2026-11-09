@echo off
setlocal EnableExtensions
set "REPO=%~dp0..\.."
if defined UNDERDOG_FEATURE_PROBE_OUT (
 set "OUT=%UNDERDOG_FEATURE_PROBE_OUT%"
) else (
 set "OUT=%TEMP%\underdog_feature_probe_v01"
)
if not exist "%OUT%" mkdir "%OUT%"
set "DXC=%ProgramFiles(x86)%\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe"
if not exist "%DXC%" (echo No DXC found & exit /b 3)
"%DXC%" -T cs_6_1 -E main -O2 -Fo "%OUT%\wave_sm61_v01.cso" "%REPO%\tools\diagnostics\wave_sm61_v01.hlsl"
if errorlevel 1 exit /b %errorlevel%
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
cl.exe /nologo /std:c++17 /EHsc /W4 /O2 /Fo"%OUT%\wave_sm61_dispatch_v01.obj" /Fe:"%OUT%\wave_sm61_dispatch_v01.exe" "%REPO%\tools\diagnostics\wave_sm61_dispatch_v01.cpp" /link d3d12.lib dxgi.lib d3dcompiler.lib
if errorlevel 1 exit /b %errorlevel%
"%OUT%\wave_sm61_dispatch_v01.exe" "%OUT%\wave_sm61_v01.cso"
exit /b %errorlevel%
