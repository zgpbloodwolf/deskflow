@echo off
chcp 65001 >nul 2>&1
REM Start deskflow-core server (debug mode)
REM Usage: cmake --build build --target run

REM Kill old processes
taskkill /f /im deskflow-core.exe >nul 2>&1
timeout /t 1 /nobreak >nul

REM Start (WORKING_DIRECTORY is set to CMAKE_SOURCE_DIR by CMake)
echo Starting deskflow-core server...
"%cd%\build\bin\deskflow-core.exe" --server -f --debug DEBUG
