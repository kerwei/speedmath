#!/usr/bin/env bash
# 迁移工具集成测试 (qiānyí gōngjù jíchéng cèshì — v1→v2 migration tool integration test)
# 构建 v1 数据库 → 迁移 → 校验 (schema/数据/HMAC 索引) → 再次运行验证 guard
#
# Usage:
#   xmake build db_migrate      # first, if not already built
#   ./scripts/test-db-migration.sh

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MIGRATE="$ROOT/build/linux/x86_64/release/db_migrate"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

KEY="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
DB="$TMP/v1.db"
CONF="$TMP/conf"

if [ ! -x "$MIGRATE" ]; then
  echo "db_migrate not built — run: xmake build db_migrate"
  exit 1
fi
printf "port=9999\nenc_key=%s\n" "$KEY" > "$CONF"

# ── 构建 v1 数据库 (gòujiàn v1 shùjùkù — build a v1 DB) ──
python3 - "$DB" <<'PY'
import sqlite3, sys
c = sqlite3.connect(sys.argv[1])
c.executescript("""
CREATE TABLE users (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  email TEXT UNIQUE NOT NULL,
  password_hash TEXT NOT NULL,
  created_at TEXT DEFAULT (datetime('now'))
);
INSERT INTO users (email, password_hash, created_at) VALUES
 ('alice@example.com', 'quickmath_seed$dbbde6f4d0087d38', '2026-01-01 00:00:00'),
 ('BOB@Example.COM ', 'somesalt$1234abcd', '2026-02-02 00:00:00'),
 ('carol@example.com', 'quickmath_seed$dbbde6f4d0087d38', '2026-03-03 00:00:00');
""")
c.commit()
PY

# ── 首次迁移 (shǒucì qiānyí — first run) ──
if ! "$MIGRATE" "$DB" --config "$CONF" >/dev/null 2>&1; then
  echo "FAIL: first migration exited non-zero"
  exit 1
fi
echo "PASS: first migration ran"

# ── 再次运行 → 必须跳过 (zài cì yùnxíng — second run must be a no-op) ──
OUT=$("$MIGRATE" "$DB" --config "$CONF" 2>&1) || { echo "FAIL: second run exited non-zero"; exit 1; }
echo "$OUT" | grep -q "already at v2" || { echo "FAIL: idempotent guard not triggered: $OUT"; exit 1; }
echo "PASS: idempotent guard (second run skipped)"

# ── 校验结果 (jiàoyàn jiéguǒ — verify schema + data via python) ──
python3 - "$DB" "$KEY" <<'PY'
import sqlite3, sys, hmac, hashlib
db, keyhex = sys.argv[1], sys.argv[2]
key = bytes.fromhex(keyhex)
c = sqlite3.connect(db)
cols = [r[1] for r in c.execute("PRAGMA table_info(users)")]
assert "email" not in cols and "email_hash" in cols and "username" in cols, f"bad schema: {cols}"
rows = [r for r in c.execute("SELECT id, email_hash, password_hash, username, created_at, username_updated_at FROM users")]
assert len(rows) == 3, f"expected 3 rows, got {len(rows)}"
expected = {
    "alice@example.com": ("quickmath_seed$dbbde6f4d0087d38", "2026-01-01 00:00:00"),
    "bob@example.com":   ("somesalt$1234abcd", "2026-02-02 00:00:00"),   # 大小写+空格已规范化 (normalized)
    "carol@example.com": ("quickmath_seed$dbbde6f4d0087d38", "2026-03-03 00:00:00"),
}
for email, (pw, created) in expected.items():
    h = hmac.new(key, email.encode(), hashlib.sha256).hexdigest()
    found = [r for r in rows if r[1] == h]
    assert len(found) == 1, f"no/duplicate row for {email}"
    r = found[0]
    assert r[2] == pw, f"password_hash changed for {email}"
    assert r[4] == created, f"created_at changed for {email}"
    assert r[3].startswith("player"), f"no random username for {email}"
    assert r[5] is None, f"username_updated_at should be NULL for {email}"
print("PASS: schema + data verified (encrypted, indexed, usernames, fields preserved)")
PY
[ $? -eq 0 ] || { echo "FAIL: data verification"; exit 1; }

echo "OK: v1→v2 migration integration test passed"
