@echo off
setlocal EnableExtensions
set "REPO=%~dp0..\.."
if defined UNDERDOG_BINDING_TEXTURE_OUT (
 set "OUT=%UNDERDOG_BINDING_TEXTURE_OUT%"
) else (
 set "OUT=%TEMP%\underdog_binding_texture_v01"
)
if not exist "%OUT%" mkdir "%OUT%"
set "DXC=%ProgramFiles(x86)%\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe"
"%DXC%" -T cs_6_1 -E main -O2 -Fo "%OUT%\tier3_sparse_uav_v01.cso" "%REPO%\tools\diagnostics\tier3_sparse_uav_v01.hlsl"
if errorlevel 1 exit /b 8
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 9
cl /nologo /std:c++17 /EHsc /W4 /O2 /Fo"%OUT%\tier3_sparse_uav_v01.obj" /Fe:"%OUT%\tier3_sparse_uav_v01.exe" "%REPO%\tools\diagnostics\tier3_sparse_uav_v01.cpp" d3d12.lib dxgi.lib d3dcompiler.lib
if errorlevel 1 exit /b 10
"%OUT%\tier3_sparse_uav_v01.exe" "%OUT%\tier3_sparse_uav_v01.cso"
exit /b %errorlevel%
