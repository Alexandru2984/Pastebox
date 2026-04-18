# 📦 PasteBox

A fast, minimal pastebin / code sharing app with syntax highlighting.

**Backend:** Drogon (C++) + SQLite3  
**Frontend:** Svelte + Tailwind CSS + highlight.js

![Stack](https://img.shields.io/badge/C++-Drogon-blue?logo=cplusplus)
![Stack](https://img.shields.io/badge/Frontend-Svelte-orange?logo=svelte)
![Stack](https://img.shields.io/badge/DB-SQLite3-green?logo=sqlite)

## Features

- ✍️ Create pastes with title, language selection, and Ctrl+Enter shortcut
- 🔍 Syntax highlighting for 30+ languages
- 📋 Copy to clipboard / 🔗 Share link / ⬇️ Download
- 📃 Recent pastes list with relative timestamps
- 👁️ View counter per paste
- 🌙 Dark theme (GitHub-style)

## Project Structure

```
pastebox/
├── backend/                 # Drogon C++ API
│   ├── controllers/         # REST controllers
│   │   ├── PasteController.h
│   │   └── PasteController.cc
│   ├── main.cc              # Entry point + DB init
│   ├── config.json          # Drogon configuration
│   └── CMakeLists.txt
└── frontend/                # Svelte SPA
    ├── src/
    │   ├── App.svelte       # Main app + router
    │   ├── PasteView.svelte # Paste viewer with highlighting
    │   ├── PasteList.svelte # Recent pastes list
    │   └── lib/             # API client + constants
    └── vite.config.js
```

## Prerequisites

- **CMake** ≥ 3.5
- **g++** with C++17 support
- **libdrogon-dev** (+ libpq-dev, libmysqlclient-dev, libbrotli-dev, etc.)
- **Node.js** ≥ 18
- **npm**

On Ubuntu/Debian:
```bash
sudo apt install cmake libdrogon-dev libpq-dev libmysqlclient-dev \
  libbrotli-dev libhiredis-dev libc-ares-dev libyaml-cpp-dev libsqlite3-dev
```

## Build & Run

### 1. Build the frontend
```bash
cd frontend
npm install
npx vite build --outDir ../backend/build/public --emptyOutDir
```

### 2. Build the backend
```bash
cd backend/build
cmake ..
make -j$(nproc)
```

### 3. Run
```bash
cd backend/build
./backend
```

The app will be available at **http://localhost:7777**

## API

| Method   | Endpoint           | Description          |
|----------|--------------------|----------------------|
| `POST`   | `/api/pastes`      | Create a new paste   |
| `GET`    | `/api/pastes`      | List recent pastes   |
| `GET`    | `/api/pastes/:id`  | Get paste by ID      |
| `DELETE` | `/api/pastes/:id`  | Delete a paste       |

### Example

```bash
# Create a paste
curl -X POST http://localhost:7777/api/pastes \
  -H 'Content-Type: application/json' \
  -d '{"title":"Hello","content":"print(42)","language":"python"}'

# Get it back
curl http://localhost:7777/api/pastes/<id>
```

## Configuration

Edit `backend/config.json` to change:
- **Port** (default: 7777)
- **Thread count**
- **Max body size**
- **Log level**

## License

MIT
