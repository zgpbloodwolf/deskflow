@echo off
REM 启动 deskflow-core server (调试模式)
REM 用法: cmake --build build --target run

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..

REM 杀掉旧进程
taskkill /f /im deskflow-core.exe >nul 2>&1
timeout /t 1 /nobreak >nul

REM 启动
echo 启动 deskflow-core server...
"%PROJECT_DIR%\build\bin\deskflow-core.exe" --server -f --debug DEBUG
