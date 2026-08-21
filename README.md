# QuickMath 🧮

A C++20 mental arithmetic trainer with a web frontend. Supports all four basic operators with configurable difficulty, question count, and operator selection. Now with AI opponents and multiplayer infrastructure.

## Project Structure

```
├── xmake.lua              # 构建配置 (gòujiàn pèizhì — build config)
├── main.cpp               # 控制台入口 (kòngzhìtái rùkǒu — console entry point)
├── quickmath.conf.example # 后端配置示例 (hòuduān pèizhì shìlì — backend config sample)
├── backend/               # HTTP 后端: REST API + WebSocket 多人游戏 (hòuduān — backend server)
├── frontend/              # Vue 3 SPA 前端 (qiánduān — frontend)
├── libquickmath/          # 核心游戏逻辑静态库 (héxīn yóuxì luóji — core game logic, C++20)
├── scripts/               # 开发 / 测试 / WSL 网络配置脚本 (kāifā/cèshì — dev, test & WSL network scripts)
├── tests/                 # doctest 单元测试 + 数据库测试 (dānyuán cèshì — unit tests)
├── deploy/                # ECS 部署脚本 + nginx/systemd 配置 (bùshǔ — deployment)
├── .github/workflows/     # CI/CD: GitHub Actions → Aliyun ECS
└── build/                 # 构建输出 (gòujiàn shūchū — build output)
```

## Quick Start (Web — Full Stack)

### Prerequisites

```bash
# Install Node.js (if not already)
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.4/install.sh | bash
source ~/.bashrc
nvm install --lts

# Install SQLite development headers
sudo apt install libsqlite3-dev

# Download cpp-httplib header
curl -o backend/httplib.h https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
```

### Build & Run

```bash
# Terminal 1: build & start backend
xmake build backend
./build/linux/x86_64/release/backend

# Terminal 2: build & start frontend dev server
cd frontend && npm install && npm run dev
```

Open **http://localhost:5173** in your browser. The Vite dev server proxies `/api` requests to the C++ backend.

### Production Build

```bash
xmake build backend
cd frontend && npm install && npm run build
cd ..
./build/linux/x86_64/release/backend
```

The backend serves the built frontend from `frontend/dist/`. Open **http://localhost:8080**.

## Build Requirements

- xmake >= 3.0.0
- C++20-compatible compiler (GCC >= 10 / Clang >= 10)
- Node.js >= 18 (for frontend)
- libsqlite3-dev (for database)

## Build Commands

```bash
xmake                     # build all targets
xmake build backend       # build only the backend
xmake build quickmath     # build only the console version
xmake build test_db       # build database unit test
xmake run quickmath       # build and run console version
xmake run backend         # build and run backend server
xmake run test_db         # run database unit test
xmake run tests           # run all C++ unit tests
./scripts/test-session-sweep.sh   # session-sweep integration test (starts backend on :8099)
xmake clean               # clean build artifacts
xmake f -m debug          # switch to debug mode
```

## Configuration

The backend reads optional settings from `quickmath.conf` in its working directory, or from a path passed via `--config`:

```bash
./build/linux/x86_64/release/backend --config /path/to/quickmath.conf
```

If the file is missing, defaults apply. See `quickmath.conf.example` for the format (`key=value` lines, `#` comments).

| Key | Default | Description |
|---|---|---|
| `port` | `8080` | HTTP listen port |
| `session_ttl` | `1800` | Solo session TTL in seconds (stale sessions are reaped by a watchdog thread) |
| `sweep_interval` | `60` | Stale-session sweep interval in seconds |

## Deployment (Aliyun ECS)

**Image:** `aliyun_4_x64_20G_pro_alibase_20260512` (Alibaba Cloud Linux 4, package manager: `dnf`)

### One-time ECS setup

```bash
# On your ECS instance as root
# Replace <your-domain.com> with your actual domain name
bash deploy/setup-ecs.sh <your-domain.com>
```

This installs: `sqlite-devel`, **nginx**, **certbot** (Let's Encrypt), creates the quickmath user, installs the systemd service, and preps `/opt/quickmath`.

> **After setup**, you still need to:
> 1. Configure DNS: point your domain's A record to the ECS IP
> 2. Get an SSL certificate: `sudo certbot --nginx -d <your-domain.com> --non-interactive --agree-tos -m admin@<your-domain.com>`
> 3. Start nginx: `sudo systemctl start nginx`

> **ECS Security Group:** After setup, go to the Aliyun web console → **ECS → Security Groups** → add inbound rules allowing **TCP 80** and **TCP 443** from `0.0.0.0/0`. Port 8080 only needs to be open if you skip nginx.

### CI/CD (GitHub Actions)

The workflow in `.github/workflows/deploy.yml` builds and deploys automatically on every push to `main`:

1. Builds C++ backend (xmake) + Vue frontend (npm)
2. Packages the binary, systemd service, and **nginx config** into a tarball
3. **DNS check** — verifies the domain resolves to the ECS IP before deploying
4. SCPs to ECS, tests nginx config, reloads nginx, restarts the service
5. **Verify step** — checks local backend, nginx status, and external HTTPS endpoint

**Required GitHub secrets (on the PROD environment):**

| Secret | Value |
|---|---|
| `ECS_SSH_HOST` | Your ECS IP address (e.g. `123.456.789.0`) |
| `ECS_SSH_KEY` | Private SSH key for root access to the ECS instance |
| `ENC_KEY` | **Required** — hex key (`openssl rand -hex 32`) used to encrypt user emails (AES-256-GCM) and derive the HMAC lookup index. Provisioned into `/opt/quickmath/quickmath.conf` on first deploy; the deploy never overwrites an existing config |
| `DOMAIN_NAME` | (Optional) Your domain name (e.g. `quickmath.example.com`) |

> If `DOMAIN_NAME` is not set, the DNS check and HTTPS verification are skipped. The pipeline still deploys the backend and checks the local service.

> **⚠️ `ENC_KEY` is the master key for all stored emails** (`email_enc` + `email_hash` are both derived from it). Keep it safe:
> - **Back it up** (password manager / vault) — if it's ever lost, users can't log in by email and profile emails can't be decrypted. GitHub secrets are not a backup.
> - **Never rotate it casually.** The value in `/opt/quickmath/quickmath.conf` is written once and intentionally never overwritten, so changing the GitHub secret alone has no effect on the server — and changing the key on the server without a planned re-encryption of existing rows breaks login/decryption for every user. Rotation is a deliberate migration, not a routine credential refresh.
> - **Same key for manual deploys** — `deploy/deploy.sh` reads it from the `ENC_KEY` env var; it must match the GitHub secret (they provision the same config).
> - **Local dev note:** your dev `quickmath.conf` + `quickmath.db` are encrypted with whatever key they were created with — regenerating the key only affects environments that get the new value (the first prod deploy). If you also switch your local dev config to the new key, existing dev users' logins break (HMAC index mismatch) — wipe the dev DB or keep the old key locally.

### Manual deploy (from local machine)

```bash
# Build locally
xmake build backend && cd frontend && npm run build && cd ..

# Deploy (nginx config is also deployed and nginx is reloaded)
bash deploy/deploy.sh root@<your-ecs-ip> <your-domain.com>
```

### Architecture

```
                         Aliyun ECS (1C1G)
        ┌──────────────────────────────────────┐
        │  nginx (port 80 → 443, HTTPS)         │
        │    ↓ proxy_pass                       │
        │  systemd → quickmath (port 8080)       │
        │    ├─ REST API  (/api/*)              │
        │    ├─ WebSocket (/api/ws)             │
        │    └─ Static files (/) ← frontend/dist │
        │  /opt/quickmath/                      │
        │    ├─ backend           (binary)      │
        │    ├─ quickmath.conf    (optional)    │
        │    ├─ quickmath.db      (SQLite)      │
        │    ├── frontend/        (static files) │
        │    └── backups/        (DB snapshots) │
        └──────────────────────────────────────┘
```

**Traffic flow:** User → `https://<domain>` → nginx (TLS termination) → `127.0.0.1:8080` → backend

## API Endpoints

| Method | Path | Auth | Description |
|---|---|---|---|
| POST | `/api/auth/register` | No | Register new user (email + password) |
| POST | `/api/auth/login` | No | Login, returns auth token |
| POST | `/api/room/create` | Yes | Create a room, returns 4-char code |
| POST | `/api/room/join` | Yes | Join a room by code |
| POST | `/api/room/leave` | Yes | Leave a room |
| POST | `/api/room/ready` | Yes | Toggle ready status |
| GET | `/api/room/status` | Yes | Get room info + players + ready states |
| POST | `/api/game/new` | No | Create a new game session |
| POST | `/api/game/question` | No | Get the next question |
| POST | `/api/game/answer` | No | Submit an answer |
| GET | `/api/game/results` | No | Get final results and clean up session |

## Roadmap

> 详细路线图 (xiángxì lùxiàntú — detailed plan): see **[ROADMAP.md](ROADMAP.md)** — mobile-first UI + animation foundation + PWA are next.

---

<div align="center">
  <img src="https://github.com/deepseek-ai/DeepSeek-V2/blob/main/figures/logo.svg?raw=true" width="240" alt="DeepSeek Logo" />
  <br/>
  <sub><i>Powered by <strong>DeepSeek-V4-Flash</strong></i></sub>
</div>
