@echo off
REM Start Deskflow GUI
REM Usage: cmake --build build --target run-gui

REM Kill old processes
taskkill /f /im deskflow.exe >nul 2>&1
taskkill /f /im deskflow-core.exe >nul 2>&1
ping -n 2 127.0.0.1 >nul

REM Start (WORKING_DIRECTORY is set to CMAKE_SOURCE_DIR by CMake)
REM CMAKE_RUNTIME_OUTPUT_DIRECTORY is build/bin, MSVC puts exe in build/bin/Release or build/bin/Debug
echo Starting Deskflow GUI...
if exist "%cd%\build\bin\Release\deskflow.exe" (
    start "" "%cd%\build\bin\Release\deskflow.exe"
) else if exist "%cd%\build\bin\Debug\deskflow.exe" (
    start "" "%cd%\build\bin\Debug\deskflow.exe"
) else if exist "%cd%\build\bin\deskflow.exe" (
    start "" "%cd%\build\bin\deskflow.exe"
) else (
    echo ERROR: deskflow.exe not found in build\bin\, build\bin\Release\ or build\bin\Debug\
    exit /b 1
)
echo Done
