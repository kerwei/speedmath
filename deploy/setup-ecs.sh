#!/bin/bash
# 部署初始化 (bùshǔ chūshǐhuà — ECS first-time setup)
# Alibaba Cloud Linux 4 (aliyun_4_x64_20G_pro_alibase_20260512)
set -euo pipefail

echo "=== Updating system packages ==="
sudo dnf update -y

echo "=== Installing dependencies ==="
sudo dnf install -y sqlite-devel

echo "=== Creating speedmath user ==="
sudo id -u speedmath &>/dev/null || sudo useradd -r -s /sbin/nologin -d /opt/speedmath speedmath

echo "=== Creating /opt/speedmath ==="
sudo mkdir -p /opt/speedmath
sudo chown speedmath:speedmath /opt/speedmath

echo "=== Installing systemd service ==="
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
sudo cp "$SCRIPT_DIR/speedmath.service" /etc/systemd/system/speedmath.service
sudo systemctl daemon-reload

echo "=== Opening firewall port 8080 ==="
if sudo firewall-cmd --state &>/dev/null; then
  sudo firewall-cmd --permanent --add-port=8080/tcp
  sudo firewall-cmd --reload
  echo "firewalld: port 8080 opened"
else
  echo "firewalld not running, skipping"
fi

echo ""
echo "=== Setup complete! ==="
echo "Next steps:"
echo "  1. Deploy the app via CI/CD pipeline:"
echo "     bash deploy/deploy.sh speedmath@<your-ecs-ip>"
echo "  2. Or manually after first deploy:"
echo "     sudo systemctl enable speedmath"
echo "     sudo systemctl start speedmath"
echo "  3. Check status: sudo systemctl status speedmath"
