#!/bin/bash
# 部署初始化 (bùshǔ chūshǐhuà — ECS first-time setup)
# Alibaba Cloud Linux 4 (aliyun_4_x64_20G_pro_alibase_20260512)
set -euo pipefail

DOMAIN="${1:-quickmath.games}"  # Default: quickmath.games

echo "=== Updating system packages ==="
sudo dnf update -y

echo "=== Installing dependencies ==="
sudo dnf install -y sqlite-devel nginx

# certbot and python3-certbot-nginx are in EPEL on Alibaba Cloud Linux 4
# Enable EPEL first, try dnf, fall back to pip3
if ! sudo dnf install -y epel-release 2>/dev/null; then
  echo "epel-release not found in repos — will install certbot via pip3"
fi
if ! sudo dnf install -y certbot python3-certbot-nginx 2>/dev/null; then
  echo "Installing certbot via pip3..."
  sudo dnf install -y python3-pip
  sudo pip3 install certbot certbot-nginx 2>/dev/null
fi

echo "=== Creating quickmath user ==="
sudo id -u quickmath &>/dev/null || sudo useradd -r -s /sbin/nologin -d /opt/quickmath quickmath

echo "=== Creating /opt/quickmath ==="
sudo mkdir -p /opt/quickmath
sudo chown quickmath:quickmath /opt/quickmath

echo "=== Installing systemd service ==="
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
sudo cp "$SCRIPT_DIR/quickmath.service" /etc/systemd/system/quickmath.service
sudo systemctl daemon-reload

echo "=== Installing nginx config for $DOMAIN ==="
sudo cp "$SCRIPT_DIR/nginx.conf" /etc/nginx/conf.d/quickmath.conf

# 自签名证书回退 (zì qiānmíng zhèngshū huí tuì — self-signed cert fallback)
# Allows nginx to start before certbot has been run.
# Stored in /etc/nginx/ssl/ to avoid conflicting with certbot's paths.
FALLBACK_KEY="/etc/nginx/ssl/$DOMAIN.key"
FALLBACK_CRT="/etc/nginx/ssl/$DOMAIN.crt"
if [ ! -f "$FALLBACK_CRT" ]; then
  echo "Generating self-signed SSL fallback for $DOMAIN..."
  sudo mkdir -p /etc/nginx/ssl
  sudo openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
    -keyout "$FALLBACK_KEY" \
    -out "$FALLBACK_CRT" \
    -subj "/CN=$DOMAIN" 2>/dev/null
  echo "Self-signed certificate created at $FALLBACK_CRT"
fi

# 真实证书检测 (zhēnshí zhèngshū jiǎncè — detect real LE cert)
# If certbot has already been run, use the real certificate instead.
# Prefer suffixed paths (e.g. $DOMAIN-0001) — certbot creates these when
# a stale directory blocks the standard path.
LE_CERT=""
LE_KEY=""
LE_DIR=$(ls -d /etc/letsencrypt/live/$DOMAIN-* 2>/dev/null | sort | tail -1)
if [ -n "$LE_DIR" ]; then
  LE_CERT="$LE_DIR/fullchain.pem"
  LE_KEY="$LE_DIR/privkey.pem"
fi
if [ -z "$LE_CERT" ] || [ ! -f "$LE_CERT" ]; then
  if [ -f /etc/letsencrypt/live/$DOMAIN/fullchain.pem ]; then
    LE_CERT="/etc/letsencrypt/live/$DOMAIN/fullchain.pem"
    LE_KEY="/etc/letsencrypt/live/$DOMAIN/privkey.pem"
  fi
fi
if [ -n "$LE_CERT" ] && [ -f "$LE_CERT" ]; then
  echo "Real LE certificate found at $LE_CERT — updating nginx config"
  sudo sed -i "s|ssl_certificate .*|ssl_certificate     $LE_CERT;|" /etc/nginx/conf.d/quickmath.conf
  sudo sed -i "s|ssl_certificate_key .*|ssl_certificate_key $LE_KEY;|" /etc/nginx/conf.d/quickmath.conf
fi

echo ""
echo "=== SSL certificate setup ==="
echo "Run the following command to obtain a Let's Encrypt certificate:"
echo "  sudo certbot --nginx -d $DOMAIN --non-interactive --agree-tos -m admin@$DOMAIN"
echo ""
echo "Or set up SSL manually with your own certificate."

echo ""
echo "=== Enabling services ==="
sudo systemctl enable nginx
echo "  nginx enabled"

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
echo "Next steps:"
echo "  1. Make sure your DNS A record for $DOMAIN points to this ECS IP"
echo "  2. Start nginx: sudo systemctl start nginx"
echo "  3. Get Let's Encrypt cert (replaces self-signed):"
echo "     sudo certbot --nginx -d $DOMAIN --non-interactive --agree-tos -m admin@$DOMAIN"
echo "  4. Deploy the app via CI/CD pipeline, then:"
echo "     sudo systemctl start quickmath"
echo "  5. Check status: systemctl status quickmath"
