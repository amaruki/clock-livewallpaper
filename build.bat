@echo off
setlocal
echo Attempting to build with MinGW (g++)...

:: Check if g++ exists
where g++ >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: 'g++' not found. Please ensure MinGW/GCC is in your PATH.
    pause
    exit /b 1
)

:: Build Command
set EXE_NAME=DesktopClock.exe
g++ main.cpp -o %EXE_NAME% -O2 -mwindows -municode -ld2d1 -ldwrite -ldwmapi -lole32 -lgdi32 -lshlwapi

if %errorlevel% equ 0 (
    echo Build Successful!
) else (
    echo Build Failed.
)

pause