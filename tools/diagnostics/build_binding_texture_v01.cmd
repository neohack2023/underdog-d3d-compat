@echo off
setlocal EnableExtensions
set "REPO=%~dp0..\.."
if defined UNDERDOG_BINDING_TEXTURE_OUT (
 set "OUT=%UNDERDOG_BINDING_TEXTURE_OUT%"
) else (
 set "OUT=%TEMP%\underdog_binding_texture_v01"
)
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OUT%" mkdir "%OUT%"
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /W3 /O2 /Fo"%OUT%\bcn_sample.obj" /Fe:"%OUT%\bcn_sample.exe" "%REPO%\examples\texture_stream\bcn_sample_v02.cpp" d3d12.lib dxgi.lib d3dcompiler.lib
if errorlevel 1 exit /b 10
cl /nologo /std:c++17 /EHsc /W3 /O2 /Fo"%OUT%\bcn_sample_v03.obj" /Fe:"%OUT%\bcn_sample_v03.exe" "%REPO%\examples\texture_stream\bcn_sample_v03.cpp" d3d12.lib dxgi.lib d3dcompiler.lib
if errorlevel 1 exit /b 14
cl /nologo /std:c++17 /EHsc /W3 /O2 /Fo"%OUT%\bcn_sample_v04.obj" /Fe:"%OUT%\bcn_sample_v04.exe" "%REPO%\examples\texture_stream\bcn_sample_v04.cpp" d3d12.lib dxgi.lib d3dcompiler.lib
if errorlevel 1 exit /b 15
cl /nologo /std:c++17 /EHsc /W3 /O2 /Fo"%OUT%\bc_mip.obj" /Fe:"%OUT%\bc_mip.exe" "%REPO%\examples\texture_stream\bc_mip_v01.cpp" d3d12.lib dxgi.lib
if errorlevel 1 exit /b 11
cl /nologo /std:c++17 /EHsc /W3 /O2 /Fo"%OUT%\bc_mip_v02.obj" /Fe:"%OUT%\bc_mip_v02.exe" "%REPO%\examples\texture_stream\bc_mip_v02.cpp" d3d12.lib dxgi.lib
if errorlevel 1 exit /b 13
cl /nologo /std:c++17 /EHsc /W3 /O2 /I"%REPO%\include" /Fo"%OUT%\typed_uav.obj" /Fe:"%OUT%\typed_uav.exe" "%REPO%\examples\diagnostics\typed_uav_bindings_v01.cpp" d3d12.lib dxgi.lib d3dcompiler.lib
if errorlevel 1 exit /b 12
echo REUSED_FIXTURE_BUILD_PASS
exit /b 0
