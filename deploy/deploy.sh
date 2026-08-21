#!/bin/bash
# 部署脚本 (bùshǔ jiǎoběn — remote deploy script)
# Ships CI-built binaries (static ICU + static libstdc++/libgcc — ABI-safe on AL4).
# No server-side compile: building on the 1C1G instance exceeded its CPU/disk limits
# (2026-08-18). Usage: deploy.sh <user@host>   (ENC_KEY env var required on first deploy)
set -euo pipefail

if [ $# -lt 1 ]; then
  echo "Usage: $0 <user@host>"
  echo "  ENC_KEY=<hex> required for the first deploy (provisions quickmath.conf)"
  echo "  Prereq: build/linux/x86_64/release/backend + db_migrate must exist, built with"
  echo "  static ICU/libstdc++ on a glibc<=2.35 toolchain (use the CI artifact or a 22.04 build)"
  exit 1
fi

HOST="$1"
ARCHIVE="/tmp/quickmath-deploy.tar.gz"
REL="build/linux/x86_64/release"

echo "=== Packaging built artifacts ==="
if [ ! -x "$REL/backend" ] || [ ! -x "$REL/db_migrate" ]; then
  echo "ERROR: $REL/backend or $REL/db_migrate missing — build them first (see CI 'Build backend' step)"
  exit 1
fi
if [ ! -d frontend/dist ]; then
  echo "ERROR: frontend/dist missing — run: cd frontend && npm run build"
  exit 1
fi

tar czf "$ARCHIVE" \
  "$REL/backend" \
  "$REL/db_migrate" \
  scripts/migrate-db.sh \
  deploy/quickmath.service \
  deploy/nginx.conf \
  frontend/dist

echo "=== Copying to ECS: $HOST ==="
scp "$ARCHIVE" "$HOST:/tmp/"

echo "=== Extracting and installing on remote ==="
# 传入 ENC_KEY 环境变量 (chuánrù huánjìng biànliàng — pass ENC_KEY to the remote shell)
ssh "$HOST" "ENC_KEY='${ENC_KEY:-}' bash -s" << 'REMOTE'
  set -euo pipefail
  cd /opt/quickmath

  # ── Step 1: check service state (informational) ──
  if systemctl is-active --quiet quickmath 2>/dev/null; then
    echo "Service is running"
  else
    echo "Service is not running"
  fi

  # ── Step 2: extract the package into a staging dir (does NOT touch the live service) ──
  STAGE=$(mktemp -d /tmp/qm-stage.XXXXXX)
  sudo tar xzf /tmp/quickmath-deploy.tar.gz -C "$STAGE"
  sudo chmod +x "$STAGE/scripts/migrate-db.sh"
  REL="$STAGE/build/linux/x86_64/release"

  # ── Step 3: provision enc_key config (never overwrite an existing key) ──
  if [ ! -f /opt/quickmath/quickmath.conf ]; then
    if [ -z "${ENC_KEY:-}" ]; then
      echo "ERROR: ENC_KEY env var not set — cannot provision quickmath.conf"
      exit 1
    fi
    printf 'port=8080\nsession_ttl=1800\nsweep_interval=60\nenc_key=%s\n' "$ENC_KEY" | sudo tee /opt/quickmath/quickmath.conf > /dev/null
    sudo chown quickmath:quickmath /opt/quickmath/quickmath.conf
    echo "quickmath.conf created with enc_key"
  else
    echo "quickmath.conf already present — keeping existing enc_key"
  fi

  # ── Step 4: smoke-test the NEW binary BEFORE stopping the live service + rollback copy ──
  # Output captured (not piped) so the check is pipefail-proof — backend's FATAL exit code is expected.
  S=$(mktemp -d)
  OUT=$(cd "$S" && timeout 10 "$REL/backend" --config /dev/null 2>&1 || true)
  if ! echo "$OUT" | grep -q "enc_key"; then
    echo "ERROR: new backend binary cannot run on this server (ABI mismatch?) — aborting; live binary untouched"
    echo "$OUT"
    rm -rf "$S"
    exit 1
  fi
  rm -rf "$S"
  echo "Smoke test passed: shipped backend runs on this server"
  if [ -f /opt/quickmath/backend ]; then
    sudo cp /opt/quickmath/backend "/opt/quickmath/backend.bak-$(date +%Y%m%d-%H%M%S)"
    ls -t /opt/quickmath/backend.bak-* 2>/dev/null | tail -n +4 | xargs sudo rm -f 2>/dev/null || true
  fi

  # ── Step 5: stop service, retire stale 8080 holders, then back up the DB (incl. WAL sidecars) ──
  # 端口清理 (duānkǒu qīnglǐ — port hygiene): SO_REUSEADDR lets a leftover process (old
  # speedmath backend) share port 8080 and answer nginx while quickmath is down. Retire it.
  sudo systemctl stop quickmath 2>/dev/null || true
  echo "Service stopped"
  if systemctl list-unit-files 2>/dev/null | grep -q '^speedmath'; then
    echo "Retiring leftover speedmath service"
    sudo systemctl stop speedmath 2>/dev/null || true
    sudo systemctl disable speedmath 2>/dev/null || true
    sudo rm -f /etc/systemd/system/speedmath.service /usr/lib/systemd/system/speedmath.service
    sudo systemctl daemon-reload
  fi
  if command -v lsof >/dev/null 2>&1; then
    HOLDERS=$(lsof -ti:8080 2>/dev/null || true)
  else
    HOLDERS=$(fuser 8080/tcp 2>/dev/null || true)
  fi
  if [ -n "$HOLDERS" ]; then
    echo "Killing stale port-8080 holder(s): $HOLDERS"
    sudo kill $HOLDERS 2>/dev/null || true
    sleep 1
    sudo lsof -ti:8080 2>/dev/null | xargs -r sudo kill 2>/dev/null || true
  else
    echo "No stale port-8080 holders"
  fi
  if [ -f /opt/quickmath/quickmath.db ]; then
    sudo mkdir -p /opt/quickmath/backups
    BK="/opt/quickmath/backups/quickmath-$(date +%Y%m%d-%H%M%S)"
    sudo cp /opt/quickmath/quickmath.db "$BK.db"
    sudo cp /opt/quickmath/quickmath.db-wal "$BK.db-wal" 2>/dev/null || true
    sudo cp /opt/quickmath/quickmath.db-shm "$BK.db-shm" 2>/dev/null || true
    ls -t /opt/quickmath/backups/quickmath-*.db | tail -n +4 | xargs sudo rm -f 2>/dev/null || true
    # 迁移以 quickmath 用户运行, backups/ 必须是 quickmath 可写 (2026-08-18 教训)
    sudo chown -R quickmath:quickmath /opt/quickmath/backups
  fi

  # ── Step 6: install the shipped binaries + migration wrapper + frontend ──
  sudo cp "$REL/backend" /opt/quickmath/backend
  sudo cp "$REL/db_migrate" /opt/quickmath/db_migrate
  sudo chmod +x /opt/quickmath/backend /opt/quickmath/db_migrate
  sudo mkdir -p /opt/quickmath/scripts
  sudo cp "$STAGE/scripts/migrate-db.sh" /opt/quickmath/scripts/migrate-db.sh
  sudo chmod +x /opt/quickmath/scripts/migrate-db.sh
  sudo cp -r "$STAGE/frontend/dist" /opt/quickmath/frontend/

  # ── Step 7: one-time v1→v2 migration (guard inside; no-op when already v2) ──
  # chown 必须先于迁移: 属主不对会导致 backups/ 与 WAL 文件写入失败
  sudo chown -R quickmath:quickmath /opt/quickmath 2>/dev/null || true
  if [ -f /opt/quickmath/quickmath.db ]; then
    sudo -u quickmath /opt/quickmath/scripts/migrate-db.sh /opt/quickmath/quickmath.db /opt/quickmath/quickmath.conf
  else
    echo "No database yet — fresh install, nothing to migrate"
  fi

  # ── Step 8: update systemd service + nginx config ──
  sudo cp "$STAGE/quickmath.service" /etc/systemd/system/quickmath.service
  sudo systemctl daemon-reload
  sudo cp "$STAGE/nginx.conf" /etc/nginx/conf.d/quickmath.conf

  # 自签名证书回退 (zì qiānmíng zhèngshū huí tuì — self-signed cert fallback)
  if [ ! -f /etc/nginx/ssl/quickmath.games.crt ]; then
    echo "Generating self-signed SSL fallback..."
    sudo mkdir -p /etc/nginx/ssl
    sudo openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
      -keyout /etc/nginx/ssl/quickmath.games.key \
      -out /etc/nginx/ssl/quickmath.games.crt \
      -subj "/CN=quickmath.games" 2>/dev/null
    echo "Self-signed certificate created at /etc/nginx/ssl/"
  fi

  # 真实证书检测 (zhēnshí zhèngshū jiǎncè — detect real LE cert)
  LE_CERT=""
  LE_KEY=""
  LE_DIR=$(ls -d /etc/letsencrypt/live/quickmath.games-* 2>/dev/null | sort | tail -1)
  if [ -n "$LE_DIR" ]; then
    LE_CERT="$LE_DIR/fullchain.pem"
    LE_KEY="$LE_DIR/privkey.pem"
  fi
  if [ -z "$LE_CERT" ] || [ ! -f "$LE_CERT" ]; then
    if [ -f /etc/letsencrypt/live/quickmath.games/fullchain.pem ]; then
      LE_CERT="/etc/letsencrypt/live/quickmath.games/fullchain.pem"
      LE_KEY="/etc/letsencrypt/live/quickmath.games/privkey.pem"
    fi
  fi
  if [ -n "$LE_CERT" ] && [ -f "$LE_CERT" ]; then
    echo "Real LE certificate found at $LE_CERT — updating nginx config"
    sudo sed -i "s|ssl_certificate .*|ssl_certificate     $LE_CERT;|" /etc/nginx/conf.d/quickmath.conf
    sudo sed -i "s|ssl_certificate_key .*|ssl_certificate_key $LE_KEY;|" /etc/nginx/conf.d/quickmath.conf
  fi

  sudo nginx -t && sudo systemctl reload nginx || sudo systemctl restart nginx
  echo "nginx configuration installed and reloaded"

  # ── Step 9: start the service ──
  sudo systemctl enable quickmath
  sudo systemctl start quickmath
  echo "Deploy complete. Service status:"
  sudo systemctl is-active quickmath

  # cleanup: drop staging + tarball
  sudo rm -f /tmp/quickmath-deploy.tar.gz
  sudo rm -rf "$STAGE"
REMOTE

rm -f "$ARCHIVE"
echo "=== Done ==="
