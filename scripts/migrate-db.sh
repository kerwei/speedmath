#!/usr/bin/env bash
# 一次性 v1→v2 数据库迁移包装器 (yīcìxìng v1→v2 qiānyí bāozhuāng qì — one-time v1→v2 DB migration wrapper)
# 用法 (usage): ./scripts/migrate-db.sh [DB_PATH] [CONF_PATH]
# 默认 (defaults): DB_PATH=$ROOT/quickmath.db, CONF_PATH=$ROOT/quickmath.conf
# 行为 (behavior):
#   - 无数据库 (全新安装) / 已迁移 → 无操作, exit 0 (内置 guard, 可安全重复运行)
#   - v1 数据库 → 备份 → 调用 db_migrate 工具执行迁移
# 注意 (note): set -e — 备份或迁移失败必须中止, 绝不继续 (2026-08-18 事故教训)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DB_PATH="${1:-$ROOT/quickmath.db}"
CONF_PATH="${2:-$ROOT/quickmath.conf}"

# 迁移工具路径 (qiānyí gōngjù lùjìng — migration tool location)
# 优先部署布局的扁平路径 (/opt/quickmath/db_migrate), 回退仓库构建树 (build/linux/.../db_migrate)
# Deploy copies the binary to the flat path and deletes build/ — the build-tree path only
# exists in a dev checkout, so the deployed layout must resolve first.
MIGRATE="$ROOT/db_migrate"
[ -x "$MIGRATE" ] || MIGRATE="$ROOT/build/linux/x86_64/release/db_migrate"

if [ ! -f "$DB_PATH" ]; then
  echo "[migrate-db] no database at $DB_PATH — nothing to migrate (fresh install)"
  exit 0
fi
if [ ! -x "$MIGRATE" ]; then
  echo "[migrate-db] db_migrate not found (looked for $ROOT/db_migrate and $ROOT/build/linux/x86_64/release/db_migrate) — run: xmake build db_migrate"
  exit 1
fi
if [ ! -f "$CONF_PATH" ]; then
  echo "[migrate-db] config not found at $CONF_PATH (needed for enc_key)"
  exit 1
fi

# 备份 (bèifèn — backup); 失败必须中止 (2026-08-18 教训: 备份失败仍继续会误导排障)
TS="$(date +%Y%m%d-%H%M%S)"
BK_DIR="$(dirname "$DB_PATH")/backups"
mkdir -p "$BK_DIR"
BACKUP="$BK_DIR/$(basename "$DB_PATH")-$TS"
cp "$DB_PATH" "$BACKUP" || { echo "[migrate-db] FAILED to create backup at $BACKUP — aborting (database untouched)"; exit 1; }
echo "[migrate-db] backed up database to $BACKUP"

# 运行迁移 (yùnxíng qiānyí — run; 工具内部自带 schema guard)
if ! "$MIGRATE" "$DB_PATH" --config "$CONF_PATH"; then
  echo "[migrate-db] migration failed — database untouched; backup at $BACKUP"
  exit 1
fi
echo "[migrate-db] done (migrated or already at v2)"
