#!/bin/bash
# 部署初始化 (bùshǔ chūshǐhuà — ECS first-time setup)
# Run this ONCE on your Aliyun ECS instance after provisioning.
set -euo pipefail

echo "=== Updating system packages ==="
sudo apt update && sudo apt upgrade -y

echo "=== Installing dependencies ==="
sudo apt install -y libsqlite3-dev

echo "=== Creating speedmath user ==="
sudo id -u speedmath &>/dev/null || sudo useradd -r -s /bin/false -d /opt/speedmath speedmath

echo "=== Creating /opt/speedmath ==="
sudo mkdir -p /opt/speedmath
sudo chown speedmath:speedmath /opt/speedmath

echo "=== Installing systemd service ==="
# The speedmath.service file should be in the same directory as this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
sudo cp "$SCRIPT_DIR/speedmath.service" /etc/systemd/system/speedmath.service
sudo systemctl daemon-reload

echo "=== Opening firewall port 8080 ==="
sudo ufw allow 8080/tcp 2>/dev/null || echo "ufw not available, skipping"

echo ""
echo "Setup complete! Next steps:"
echo "  1. Deploy the app via CI/CD pipeline"
echo "  2. sudo systemctl start speedmath"
echo "  3. sudo systemctl enable speedmath"
echo ""
echo "To check status: sudo systemctl status speedmath"
