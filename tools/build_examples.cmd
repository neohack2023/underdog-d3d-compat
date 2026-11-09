@echo off
setlocal EnableExtensions
where cl >nul 2>nul
if errorlevel 1 (
  if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
)
where cl >nul 2>nul
if errorlevel 1 exit /b 2
cd /d "%~dp0.."
if not exist build mkdir build
cl /nologo /std:c++17 /EHsc /W3 /O1 /Iinclude /Fe:build\underdog_independent_present.exe examples\present\independent_present_v01.cpp d3d12.lib dxgi.lib user32.lib
if errorlevel 1 exit /b 10
cl /nologo /std:c++17 /EHsc /W3 /O1 /Iinclude /Fe:build\underdog_bcn_sample.exe examples\texture_stream\bcn_sample_v02.cpp d3d12.lib dxgi.lib d3dcompiler.lib
if errorlevel 1 exit /b 11
echo Selected independent application builds passed.
