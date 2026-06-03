@echo off
REM 启动 Deskflow GUI
REM 用法: cmake --build build --target run-gui

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..

REM 杀掉旧进程
taskkill /f /im Deskflow.exe >nul 2>&1
taskkill /f /im deskflow-core.exe >nul 2>&1
timeout /t 1 /nobreak >nul

REM 启动
echo 启动 Deskflow GUI...
start "" "%PROJECT_DIR%\build\bin\Deskflow.exe"
echo 已启动
