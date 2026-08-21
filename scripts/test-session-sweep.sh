#!/usr/bin/env bash
# 会话清理集成测试 (huìhuà qīnglǐ jíchéng cèshì — session sweep integration test)
# Starts the backend with a short TTL (via a temp config file) on a test port,
# creates a solo game session, then verifies the watchdog thread reaps it once
# it goes idle past the TTL.
#
# Usage:
#   xmake build backend            # first, if not already built
#   ./scripts/test-session-sweep.sh
#
# Env overrides (defaults below): TEST_PORT, TEST_TTL (seconds), TEST_SWEEP (seconds)

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${TEST_PORT:-8099}"
TTL="${TEST_TTL:-2}"          # 会话过期时间 (huìhuà guòqī shíjiān — session TTL in seconds)
SWEEP="${TEST_SWEEP:-1}"      # 清理周期 (qīnglǐ zhōuqī — sweep interval in seconds)

BACKEND="$ROOT/build/linux/x86_64/release/backend"
if [ ! -x "$BACKEND" ]; then
  echo "backend not built — run: xmake build backend"
  exit 1
fi

cd "$ROOT"
# 在临时目录中运行, 避免污染仓库内的开发数据库 (run in a temp dir so the repo's dev DB stays untouched)
WORK="$(mktemp -d)"
# 临时配置文件 (línshí pèizhì wénjiàn — temp config file with a short TTL)
TMP_CONF="$WORK/conf"
cat > "$TMP_CONF" <<EOF
port=$PORT
session_ttl=$TTL
sweep_interval=$SWEEP
enc_key=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
EOF

( cd "$WORK" && exec "$BACKEND" --config "$TMP_CONF" ) &
SERVER_PID=$!
cleanup() {
  kill "$SERVER_PID" 2>/dev/null
  wait "$SERVER_PID" 2>/dev/null
  rm -rf "$WORK"
}
trap cleanup EXIT

# Wait for the server to accept connections (any HTTP response means it's up)
code="000"
for _ in $(seq 1 30); do
  code=$(curl -s -o /dev/null -w "%{http_code}" -m 2 "http://127.0.0.1:$PORT/api/game/results?session_id=probe")
  [ "$code" != "000" ] && break
  sleep 0.5
done
if [ "$code" = "000" ]; then
  echo "FAIL: backend did not start on port $PORT"
  exit 1
fi

# Create a solo game session
sid=$(curl -s -X POST "http://127.0.0.1:$PORT/api/game/new" \
  -d "diff=1&intense=1&ops=1234&ai_levels=" \
  | sed -E 's/.*"session_id":"([0-9a-fA-F]+)".*/\1/')
if [ -z "$sid" ]; then
  echo "FAIL: could not create a game session"
  exit 1
fi
echo "session created: $sid"

# 1) Session must be alive right after creation
code=$(curl -s -o /dev/null -w "%{http_code}" -X POST "http://127.0.0.1:$PORT/api/game/question" -d "session_id=$sid")
if [ "$code" != "200" ]; then
  echo "FAIL: session not reachable right after create (http $code)"
  exit 1
fi
echo "PASS: session alive after create (http $code)"

# 2) Wait past TTL + sweep interval, then the session must be gone
sleep $((TTL + SWEEP + 2))
code=$(curl -s -o /dev/null -w "%{http_code}" -X POST "http://127.0.0.1:$PORT/api/game/question" -d "session_id=$sid")
if [ "$code" = "404" ]; then
  echo "PASS: stale session swept after ${TTL}s TTL (http $code)"
else
  echo "FAIL: expected 404 after TTL, got $code"
  exit 1
fi

echo "OK: session-sweep integration test passed"
