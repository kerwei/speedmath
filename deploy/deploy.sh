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
ARCHIVE="/tmp/speedmath-deploy.tar.gz"
REMOTE_DIR="/opt/speedmath"

echo "=== Packaging built artifacts ==="
# Artifacts must already be built in the repo root:
#   build/linux/x86_64/release/backend
#   frontend/dist/
#   deploy/speedmath.service
tar czf "$ARCHIVE" \
  build/linux/x86_64/release/backend \
  deploy/speedmath.service

echo "=== Copying to ECS: $HOST ==="
scp "$ARCHIVE" "$HOST:/tmp/"

echo "=== Extracting and installing on remote ==="
ssh "$HOST" bash -s << 'REMOTE'
  set -euo pipefail
  sudo systemctl stop speedmath 2>/dev/null || true
  
  cd /opt/speedmath
  
  # Backup DB (keep last 3)
  if [ -f speedmath.db ]; then
    mkdir -p backups
    cp speedmath.db "backups/speedmath-$(date +%Y%m%d-%H%M%S).db"
    ls -t backups/speedmath-*.db | tail -n +4 | xargs rm -f 2>/dev/null || true
  fi
  
  # Extract new binary
  sudo tar xzf /tmp/speedmath-deploy.tar.gz -C /tmp/
  sudo cp /tmp/build/linux/x86_64/release/backend /opt/speedmath/backend
  sudo chmod +x /opt/speedmath/backend
  sudo chown speedmath:speedmath /opt/speedmath/backend
  
  # Update systemd service
  sudo cp /tmp/deploy/speedmath.service /etc/systemd/system/speedmath.service
  sudo systemctl daemon-reload
  
  # Clean up temp
  rm -rf /tmp/build /tmp/deploy /tmp/speedmath-deploy.tar.gz
  
  # Start
  sudo systemctl start speedmath
  echo "Deploy complete. Service status:"
  sudo systemctl is-active speedmath
REMOTE

rm -f "$ARCHIVE"
echo "=== Done ==="
