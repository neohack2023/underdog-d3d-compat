@echo off
setlocal
cd /d "%~dp0.."
if not exist "build\Release\underdog_arena.exe" (
 echo Missing compiled arena executable. Build with CMake first.
 pause
 exit /b 2
)
echo Underdog Arena. Use LEFT and RIGHT to move camera; Alt+F4 to quit.
"build\Release\underdog_arena.exe" 3600
if errorlevel 1 (
 echo Arena returned error %errorlevel%.
 pause
)
