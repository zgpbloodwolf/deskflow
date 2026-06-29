#!/bin/bash
# 启动 Deskflow GUI
# 用法: cmake --build build --target run-gui

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."

if [ -z "${DESKFLOW_APP:-}" ]; then
  echo "构建 Deskflow GUI..."
  cmake --build "$PROJECT_DIR/build" --target Deskflow || exit 1
fi

if [ "$(uname)" = "Darwin" ]; then
  APP="${DESKFLOW_APP:-$PROJECT_DIR/build/bin/Deskflow.app}"
else
  APP="${DESKFLOW_APP:-$PROJECT_DIR/build/bin/deskflow}"
fi

# 杀掉旧进程
pkill -f "Deskflow" 2>/dev/null || true
pkill -f "deskflow-core" 2>/dev/null || true
sleep 1

# 启动
echo "启动 Deskflow GUI..."
if [ "$(uname)" = "Darwin" ]; then
  open "$APP"
else
  nohup "$APP" > /dev/null 2>&1 &
fi
echo "已启动"
