@echo off
REM Start Deskflow GUI
REM Usage: cmake --build build --target run-gui

REM Kill old processes
taskkill /f /im deskflow.exe >nul 2>&1
taskkill /f /im deskflow-core.exe >nul 2>&1
ping -n 2 127.0.0.1 >nul

REM WORKING_DIRECTORY is set to CMAKE_SOURCE_DIR by CMake.
REM CMAKE_RUNTIME_OUTPUT_DIRECTORY is build/bin; MSVC additionally places the
REM exe under build/bin/Release or build/bin/Debug.
echo Starting Deskflow GUI...
set "EXE="
if exist "%cd%\build\bin\Release\deskflow.exe" (
    set "EXE=%cd%\build\bin\Release\deskflow.exe"
) else if exist "%cd%\build\bin\Debug\deskflow.exe" (
    set "EXE=%cd%\build\bin\Debug\deskflow.exe"
) else if exist "%cd%\build\bin\deskflow.exe" (
    set "EXE=%cd%\build\bin\deskflow.exe"
) else (
    echo ERROR: deskflow.exe not found in build\bin\, build\bin\Release\ or build\bin\Debug\
    exit /b 1
)

REM IMPORTANT: launch via PowerShell Start-Process so the child does NOT inherit
REM this console's stdio handles. Otherwise CMake's custom target keeps waiting
REM on the inherited stdout pipe to close, which makes
REM `cmake --build build --target run-gui` hang until the GUI exits.
powershell -NoProfile -Command "Start-Process -FilePath '%EXE%'"
echo Done
