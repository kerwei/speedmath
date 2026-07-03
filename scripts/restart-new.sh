#!/bin/bash
# 重启后端脚本 (chóngqǐ hòuduān jiǎoběn — restart backend script)
# Usage: ./scripts/restart-new.sh

set -e

echo "=== Step 1: 终止旧后端 (zhōngzhǐ jiù hòuduān — kill old backend) ==="
# Find and kill any process matching our backend binary
OLD_PID=$(lsof -ti:8080 2>/dev/null || true)
if [ -n "$OLD_PID" ]; then
    # Verify it's our backend before killing
    if ps -p "$OLD_PID" -o cmd= 2>/dev/null | grep -qi "backend\|speedmath"; then
        echo "发现旧进程 (found old process PID=$OLD_PID), 正在终止..."
        kill "$OLD_PID" 2>/dev/null || true
        sleep 1
        # Force kill if still alive
        if kill -0 "$OLD_PID" 2>/dev/null; then
            kill -9 "$OLD_PID" 2>/dev/null || true
        fi
        echo "旧进程已终止 (old process terminated)"
    else
        echo "端口 8080 被其他进程占用 (port 8080 held by another process), 跳过 (skipping)"
    fi
else
    echo "端口 8080 空闲 (port 8080 is free)"
fi

echo ""
echo "=== Step 2: 重新构建 (chóngxīn gòujiàn — rebuild) ==="
xmake build backend

echo ""
echo "=== Step 3: 启动后端 (qǐdòng hòuduān — start backend) ==="
# Find the built binary
BINARY=$(find build -name backend -type f 2>/dev/null | head -1)
if [ -z "$BINARY" ]; then
    echo "错误: 找不到后端二进制文件 (error: backend binary not found)"
    exit 1
fi

# Launch in foreground — press Ctrl+C to stop
echo "后端已启动: http://localhost:8080 (backend started)"
echo "按 Ctrl+C 停止 (press Ctrl+C to stop)"
echo ""
exec "$BINARY"
