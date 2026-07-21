#!/bin/bash
# 部署初始化 (bùshǔ chūshǐhuà — ECS first-time setup)
# Alibaba Cloud Linux 4 (aliyun_4_x64_20G_pro_alibase_20260512)
set -euo pipefail

DOMAIN="${1:-}"  # Optional: domain name for HTTPS setup

echo "=== Updating system packages ==="
sudo dnf update -y

echo "=== Installing dependencies ==="
sudo dnf install -y sqlite-devel nginx certbot python3-certbot-nginx

echo "=== Creating speedmath user ==="
sudo id -u speedmath &>/dev/null || sudo useradd -r -s /sbin/nologin -d /opt/speedmath speedmath

echo "=== Creating /opt/speedmath ==="
sudo mkdir -p /opt/speedmath
sudo chown speedmath:speedmath /opt/speedmath

echo "=== Installing systemd service ==="
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
sudo cp "$SCRIPT_DIR/speedmath.service" /etc/systemd/system/speedmath.service
sudo systemctl daemon-reload

echo "=== Installing nginx config ==="
if [ -z "$DOMAIN" ]; then
  echo "WARNING: No domain provided. Installing config with placeholder."
  echo "After setup, edit /etc/nginx/conf.d/speedmath.conf and replace \${DOMAIN} with your domain."
  sudo sed "s/\${DOMAIN}/your-domain.com/g" "$SCRIPT_DIR/nginx.conf" \
    | sudo tee /etc/nginx/conf.d/speedmath.conf > /dev/null
  echo "Then run: sudo certbot --nginx -d your-domain.com"
else
  sudo sed "s/\${DOMAIN}/$DOMAIN/g" "$SCRIPT_DIR/nginx.conf" \
    | sudo tee /etc/nginx/conf.d/speedmath.conf > /dev/null
  echo "nginx config created for domain: $DOMAIN"
  echo ""
  echo "=== SSL certificate setup ==="
  echo "Run the following command to obtain a Let's Encrypt certificate:"
  echo "  sudo certbot --nginx -d $DOMAIN --non-interactive --agree-tos -m admin@$DOMAIN"
  echo ""
  echo "Or set up SSL manually with your own certificate."
fi

echo ""
echo "=== Enabling services ==="
sudo systemctl enable nginx
echo "  nginx enabled (but not started — start it after SSL is configured)"

echo "=== Opening firewall ports ==="
if sudo firewall-cmd --state &>/dev/null; then
  sudo firewall-cmd --permanent --add-port=80/tcp
  sudo firewall-cmd --permanent --add-port=443/tcp
  sudo firewall-cmd --reload
  echo "firewalld: ports 80 and 443 opened"
else
  echo "firewalld not running, skipping"
fi

echo ""
echo "=== Setup complete! ==="
echo ""
echo "IMPORTANT: Open ports 80 and 443 in your Aliyun Security Group:"
echo "  ECS Console → Security Groups → Add inbound rules:"
echo "    TCP 80/80,   Source: 0.0.0.0/0"
echo "    TCP 443/443, Source: 0.0.0.0/0"
echo ""
echo "You can also close port 8080 — nginx handles external traffic now."
echo ""
if [ -n "$DOMAIN" ]; then
  echo "Next steps:"
  echo "  1. Make sure your DNS A record for $DOMAIN points to this ECS IP"
  echo "  2. Run: sudo systemctl start nginx"
  echo "  3. Run: sudo certbot --nginx -d $DOMAIN --non-interactive --agree-tos -m admin@$DOMAIN"
  echo "  4. Deploy the app via CI/CD pipeline, then:"
  echo "     sudo systemctl start speedmath"
  echo "  5. Check status: systemctl status speedmath"
fi
