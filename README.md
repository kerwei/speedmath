# SpeedMath 🧮

A C++20 mental arithmetic trainer with a web frontend. Supports all four basic operators with configurable difficulty, question count, and operator selection.

## Project Structure

```
├── xmake.lua                   # 构建配置 (gòujiàn pèizhì — build config)
├── main.cpp                    # 控制台入口 (kòngzhìtái rùkǒu — console entry point)
├── backend/
│   ├── main.cpp                # HTTP 后端服务器 (hòuduān fúwùqì — backend server)
│   └── httplib.h               # HTTP 库 (single-header)
├── frontend/
│   ├── index.html              # SPA 入口
│   ├── vite.config.js          # Vite 配置 + 代理
│   └── src/
│       ├── App.vue             # 根组件 (gēn zǔjiàn — root component)
│       ├── style.css           # 全局样式 (quánjú yàngshì — global styles)
│       └── components/
│           ├── WelcomePage.vue  # 设置页面 (shèzhì — settings)
│           ├── QuizPage.vue     # 答题页面 (dátí — quiz)
│           └── ResultsPage.vue  # 结果页面 (jiéguǒ — results)
├── libspeedmath/
│   ├── arithmetic.h / .cpp     # 运算符枚举、四则运算校验、随机数生成
│   └── manager.h / .cpp        # 游戏管理：出题、计分、计时、持久化
└── build/                      # 构建输出 (gòujiàn shūchū — build output)
```

## Quick Start (Console)

```bash
xmake
./build/linux/x86_64/release/speedmath
```

## Quick Start (Web — Full Stack)

### Prerequisites

```bash
# Install Node.js (if not already)
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.4/install.sh | bash
source ~/.bashrc
nvm install --lts

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

## How to Play (Console)

1. Choose difficulty (1 = 1-digit, 2 = 2-digit, etc.)
2. Choose intensity (1 = 10 questions, 2 = 20 questions, etc.)
3. Choose operators (e.g. `1234` for all four)
4. Answer questions. For division, input `quotient remainder` (e.g. `7 / 3` → `2 1`)
5. Results shown after all questions: time, score, average speed

## Build Requirements

- xmake >= 3.0.0
- C++20-compatible compiler (GCC >= 10 / Clang >= 10)
- Node.js >= 18 (for frontend)

## Build Commands

```bash
xmake                     # build all targets
xmake build backend       # build only the backend
xmake build speedmath     # build only the console version
xmake run                 # build and run console version
xmake clean               # clean build artifacts
xmake f -m debug          # switch to debug mode
```

## API Endpoints

| Method | Path | Description |
|---|---|---|
| POST | `/api/game/new` | Create a new game session |
| POST | `/api/game/question` | Get the next question |
| POST | `/api/game/answer` | Submit an answer |
| GET | `/api/game/results` | Get final results and clean up session |

## Roadmap

- [x] Four arithmetic operators (+ - * /)
- [x] Web frontend (Vue 3 SPA)
- [x] C++ HTTP backend (cpp-httplib)
- [ ] AI opponent mode
- [ ] Multiplayer competition
- [ ] Android port

---

<div align="center">
  <img src="https://github.com/deepseek-ai/DeepSeek-V2/blob/main/figures/logo.svg?raw=true" width="300" alt="DeepSeek Logo" />
  <br/>
  <sub><i>Powered by <strong>DeepSeek-V4-Flash</strong></i></sub>
</div>
