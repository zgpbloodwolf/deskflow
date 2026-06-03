@echo off
chcp 65001 >nul 2>&1
REM Start Deskflow GUI
REM Usage: cmake --build build --target run-gui

REM Kill old processes
taskkill /f /im Deskflow.exe >nul 2>&1
taskkill /f /im deskflow-core.exe >nul 2>&1
timeout /t 1 /nobreak >nul

REM Start (WORKING_DIRECTORY is set to CMAKE_SOURCE_DIR by CMake)
echo Starting Deskflow GUI...
start "" "%cd%\build\bin\Deskflow.exe"
echo Done
