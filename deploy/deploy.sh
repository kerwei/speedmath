#!/bin/bash
# 部署脚本 (bùshǔ jiǎoběn — remote deploy script)
# Called by GitHub Actions to deploy the built artifacts to ECS.
# Usage: deploy.sh <ssh-host>
set -euo pipefail

if [ $# -lt 1 ]; then
  echo "Usage: $0 <user@host>"
  exit 1
fi

HOST="$1"
ARCHIVE="/tmp/quickmath-deploy.tar.gz"

echo "=== Packaging built artifacts ==="
# Artifacts must already be built in the repo root:
#   build/linux/x86_64/release/backend
#   frontend/dist/
#   deploy/quickmath.service
#   deploy/nginx.conf
tar czf "$ARCHIVE" \
  build/linux/x86_64/release/backend \
  deploy/quickmath.service \
  deploy/nginx.conf

echo "=== Copying to ECS: $HOST ==="
scp "$ARCHIVE" "$HOST:/tmp/"

echo "=== Extracting and installing on remote ==="
ssh "$HOST" bash -s << 'REMOTE'
  set -euo pipefail
  sudo systemctl stop quickmath 2>/dev/null || true

  cd /opt/quickmath

  # Backup DB (keep last 3)
  if [ -f quickmath.db ]; then
    mkdir -p backups
    cp quickmath.db "backups/quickmath-$(date +%Y%m%d-%H%M%S).db"
    ls -t backups/quickmath-*.db | tail -n +4 | xargs rm -f 2>/dev/null || true
  fi

  # Extract new binary
  sudo tar xzf /tmp/quickmath-deploy.tar.gz -C /tmp/
  sudo cp /tmp/build/linux/x86_64/release/backend /opt/quickmath/backend
  sudo chmod +x /opt/quickmath/backend
  sudo chown quickmath:quickmath /opt/quickmath/backend

  # Update systemd service
  sudo cp /tmp/deploy/quickmath.service /etc/systemd/system/quickmath.service
  sudo systemctl daemon-reload

  # Update nginx config
  sudo cp /tmp/deploy/nginx.conf /etc/nginx/conf.d/quickmath.conf

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
  # Prefer suffixed paths (quickmath.games-NNNN) — certbot creates these when
  # a stale directory blocks the standard path.
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

  # Clean up temp
  rm -rf /tmp/build /tmp/deploy /tmp/quickmath-deploy.tar.gz

  # Start
  sudo systemctl start quickmath
  echo "Deploy complete. Service status:"
  sudo systemctl is-active quickmath
REMOTE

rm -f "$ARCHIVE"
echo "=== Done ==="
