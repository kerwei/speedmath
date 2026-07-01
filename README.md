# SpeedMath 🧮

A C++20 command-line mental arithmetic trainer. Supports all four basic operators with configurable difficulty, question count, and operator selection.

## Quick Start

```bash
git clone <repo-url> && cd speedmath
xmake
./build/linux/x86_64/release/speedmath
```

## Project Structure

```
├── xmake.lua                   # xmake build config
├── main.cpp                    # Entry point: user interaction loop
├── libspeedmath/
│   ├── arithmetic.h / .cpp     # Op enum, 4-operator validation, random generators
│   ├── manager.h / .cpp        # Game manager: question generation, scoring, persistence
└── build/                      # Build output
```

## How to Play

1. Choose difficulty (1 = 1-digit, 2 = 2-digit, etc.)
2. Choose intensity (1 = 10 questions, 2 = 20 questions, etc.)
3. Choose operators (e.g. `1234` for all four)
4. Answer questions. For division, input `quotient remainder` (e.g. `7 / 3` → `2 1`)
5. Results shown after all questions: time, score, average speed

## Build Requirements

- xmake >= 3.0.0
- C++20-compatible compiler (GCC >= 10 / Clang >= 10)

## Build Commands

```bash
xmake          # build
xmake run      # build and run
xmake clean    # clean build artifacts
xmake f -m debug   # switch to debug mode
```

## Roadmap

- [x] Four arithmetic operators (+ - * /)
- [ ] Web frontend (Vue 3 SPA)
- [ ] C++ HTTP backend (cpp-httplib)
- [ ] AI opponent mode
- [ ] Multiplayer competition
- [ ] Android port
