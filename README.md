# SpeedMath 🧮

A C++20 mental arithmetic trainer with a web frontend. Supports all four basic operators with configurable difficulty, question count, and operator selection. Now with AI opponents and multiplayer infrastructure.

## Project Structure

```
├── xmake.lua                   # 构建配置 (gòujiàn pèizhì — build config)
├── main.cpp                    # 控制台入口 (kòngzhìtái rùkǒu — console entry point)
├── backend/
│   ├── main.cpp                # HTTP 后端服务器 (hòuduān fúwùqì — backend server)
│   ├── db.h                    # SQLite 数据库封装 (shùjùkù fēngzhuāng — DB wrapper)
│   └── httplib.h               # HTTP 库 (single-header)
├── frontend/
│   ├── index.html              # SPA 入口
│   ├── vite.config.js          # Vite 配置 + 代理
│   └── src/
│       ├── App.vue             # 根组件 (gēn zǔjiàn — root component)
│       ├── style.css           # 全局样式 (quánjú yàngshì — global styles)
│       ├── composables/
│       │   └── useI18n.js      # 国际化 composable
│       └── locales/
│           ├── zh.json         # 中文语言包
│           └── en.json         # 英文语言包
│       └── components/
│           ├── WelcomePage.vue  # 设置页面 (shèzhì — settings)
│           ├── QuizPage.vue     # 答题页面 (dátí — quiz)
│           └── ResultsPage.vue  # 结果页面 (jiéguǒ — results)
├── libspeedmath/
│   ├── arithmetic.h / .cpp     # 运算符枚举、四则运算校验、随机数生成
│   ├── ai_opponent.h / .cpp    # AI 对手 (AI opponent with difficulty levels)
│   └── manager.h / .cpp        # 游戏管理：出题、计分、计时、持久化
├── tests/
│   ├── doctest.h               # 测试框架 (test framework)
│   ├── test_arithmetic.cpp     # 算术测试
│   ├── test_manager.cpp        # 管理器测试
│   ├── test_ai.cpp             # AI 对手测试
│   └── db_test.cpp             # 数据库测试
└── build/                      # 构建输出 (gòujiàn shūchū — build output)
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
xmake build speedmath     # build only the console version
xmake build test_db       # build database unit test
xmake run speedmath       # build and run console version
xmake run backend         # build and run backend server
xmake run test_db         # run database unit test
xmake run tests           # run all C++ unit tests
xmake clean               # clean build artifacts
xmake f -m debug          # switch to debug mode
```

## Deployment (Aliyun ECS)

**Image:** `aliyun_4_x64_20G_pro_alibase_20260512` (Alibaba Cloud Linux 4, package manager: `dnf`)

### One-time ECS setup

```bash
# On your ECS instance as root
bash deploy/setup-ecs.sh
```

This installs `sqlite-devel`, creates the speedmath user, installs the systemd service, and preps `/opt/speedmath`.

> **ECS Security Group:** After setup, go to the Aliyun web console → **ECS → Security Groups** → add an inbound rule allowing **TCP 8080** from `0.0.0.0/0`. Instance-level firewalls (firewalld/iptables) are not used on Alibaba Cloud.

### CI/CD (GitHub Actions)

The workflow in `.github/workflows/deploy.yml` builds and deploys automatically on every push to `main`:

1. Builds C++ backend (xmake) + Vue frontend (npm)
2. Packages the binary and systemd service into a tarball
3. SCPs to ECS and restarts the service

**Required GitHub secrets (on the PROD environment):**

| Secret | Value |
|---|---|
| `ECS_SSH_HOST` | Your ECS IP address (e.g. `123.456.789.0`) |
| `ECS_SSH_KEY` | Private SSH key for root access to the ECS instance |

### Manual deploy (from local machine)

```bash
# Build locally
xmake build backend && cd frontend && npm run build && cd ..

# Deploy
bash deploy/deploy.sh speedmath@<your-ecs-ip>
```

### Architecture

```
                         Aliyun ECS (1C1G)
        ┌──────────────────────────────────────┐
        │  systemd → speedmath (port 8080)      │
        │    ├─ REST API  (/api/*)              │
        │    ├─ WebSocket (/api/ws)             │
        │    └─ Static files (/) ← frontend/dist │
        │  /opt/speedmath/                      │
        │    ├─ backend           (binary)      │
        │    ├─ speedmath.db      (SQLite)      │
        │    └─ backups/         (DB snapshots) │
        └──────────────────────────────────────┘
```

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

- [x] Four arithmetic operators (+ - * /)
- [x] Web frontend (Vue 3 SPA)
- [x] C++ HTTP backend (cpp-httplib)
- [x] AI opponent mode with configurable difficulty
- [x] Internationalization (中文 / English)
- [x] SQLite database for user/room storage
- [x] User registration and login (token-based auth)
- [x] Room system (create / join / ready / leave / status)
- [x] Frontend: login/register page and room lobby UI
- [x] Real-time multiplayer (WebSocket gameplay + leaderboard sync)
- [x] Multiplayer game loop (host starts, timed answers, auto-advance)
- [x] Disconnect handling (player drop detection, answer timeout)
- [x] CI/CD pipeline (GitHub Actions → Aliyun ECS)
- [ ] Android port

---

<div align="center">
  <img src="https://github.com/deepseek-ai/DeepSeek-V2/blob/main/figures/logo.svg?raw=true" width="240" alt="DeepSeek Logo" />
  <br/>
  <sub><i>Powered by <strong>DeepSeek-V4-Flash</strong></i></sub>
</div>
