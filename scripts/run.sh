#!/bin/bash
# 启动 deskflow-core server (调试模式)
# 用法: cmake --build build --target run

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."

if [ "$(uname)" = "Darwin" ]; then
  CORE="$PROJECT_DIR/build/bin/Deskflow.app/Contents/MacOS/deskflow-core"
else
  CORE="$PROJECT_DIR/build/bin/deskflow-core"
fi

# 杀掉旧进程
pkill -f deskflow-core 2>/dev/null || true
sleep 1

# 清理日志
rm -f "$HOME/deskflow.log"

# 启动
echo "启动 deskflow-core server..."
"$CORE" server --new-instance
