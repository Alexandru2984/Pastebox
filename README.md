# 📦 PasteBox

A fast, feature-rich pastebin and code sharing app with syntax highlighting, built with a C++ backend for maximum performance.

**Live:** [pastebox.micutu.com](https://pastebox.micutu.com)

![Stack](https://img.shields.io/badge/C++-Drogon-blue?logo=cplusplus)
![Stack](https://img.shields.io/badge/Frontend-Svelte_5-orange?logo=svelte)
![Stack](https://img.shields.io/badge/DB-SQLite3-green?logo=sqlite)
![Stack](https://img.shields.io/badge/CSS-Tailwind_4-06B6D4?logo=tailwindcss)
![Tests](https://img.shields.io/badge/Tests-200_passing-brightgreen)

## Features

### Core
- ✍️ Create, edit, fork, and delete pastes
- 🔍 Syntax highlighting for 30+ languages (highlight.js)
- 📋 Copy to clipboard / 🔗 Share link / Raw view
- 📃 Browse public pastes with pagination
- 🏷️ Tag system with filtering
- 👁️ View counter per paste
- ⏰ Auto-expiration (1h, 24h, 7d, 30d)
- 🔥 Burn after read (self-destructing pastes)
- 🔒 Password protection (salted SHA-256, constant-time comparison)
- 👁️‍🗨️ Visibility controls (public, unlisted, private)
- 🍴 Fork any paste (with parent tracking)
- 📐 Embeddable `<iframe>` widget

### UI/UX
- 🌗 Multiple themes (Dark, Light, Monokai, Solarized, Nord, Dracula)
- ⌨️ Keyboard shortcuts (Ctrl+Enter to submit, Ctrl+S to save, Ctrl+Shift+N for new)
- 📱 Fully responsive / mobile-friendly
- ♿ Accessible: semantic HTML, ARIA labels, keyboard navigation, screen reader support
- 🔔 Toast notifications

### Security
- 🛡️ CSRF protection (custom header required on all mutations)
- 🚦 Rate limiting (token bucket: 10 req/s, burst 30)
- 🔐 Brute-force protection (5 failures → 5 min lockout per IP)
- 🔒 All security headers: CSP, HSTS, X-Frame-Options, Permissions-Policy, etc.
- 🎲 Cryptographic ID generation (OpenSSL RAND_bytes, rejection sampling)
- 🕵️ Anti-enumeration (password failures return 404, not 403)
- 🌐 CORS restricted to production origin
- 🔗 Backend binds to 127.0.0.1 only (nginx reverse proxy)

## Project Structure

```
pastebox/
├── backend/                    # Drogon C++ API
│   ├── controllers/
│   │   ├── PasteController.h   # Route definitions
│   │   └── PasteController.cc  # All endpoint logic + security
│   ├── main.cc                 # Entry point, DB init, rate limiting, CORS
│   ├── config.json             # Drogon configuration
│   ├── CMakeLists.txt
│   ├── test.sh                 # 93 integration tests
│   └── e2e-test.sh             # 44 E2E flow tests
├── frontend/                   # Svelte 5 SPA
│   ├── src/
│   │   ├── App.svelte          # Main app + hash router
│   │   ├── CreatePaste.svelte  # Paste creation form
│   │   ├── PasteView.svelte    # Viewer with edit mode
│   │   ├── PasteList.svelte    # Browse + pagination
│   │   ├── EmbedView.svelte    # Embeddable widget
│   │   ├── ThemeSwitcher.svelte
│   │   ├── Toast.svelte
│   │   ├── lib/
│   │   │   ├── api.js          # API client with CSRF
│   │   │   ├── languages.js    # Language definitions
│   │   │   ├── themes.js       # Theme definitions
│   │   │   └── toast.js        # Toast store
│   │   └── __tests__/          # 63 vitest tests (7 test files)
│   ├── vitest.config.js
│   └── package.json
└── .gitignore
```

## Prerequisites

- **CMake** ≥ 3.5
- **g++** with C++17 support
- **libdrogon-dev** (+ dependencies)
- **OpenSSL** development headers
- **Node.js** ≥ 18
- **npm**

On Ubuntu/Debian:
```bash
sudo apt install cmake g++ libdrogon-dev libpq-dev libmysqlclient-dev \
  libbrotli-dev libhiredis-dev libc-ares-dev libyaml-cpp-dev \
  libsqlite3-dev libssl-dev
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
mkdir -p backend/build && cd backend/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 3. Run
```bash
cd backend/build
./backend
```

The app will be available at **http://localhost:7777**

## API

All state-changing endpoints require the `X-Requested-With: PasteBox` header (CSRF protection).

| Method   | Endpoint                | Description                          |
|----------|-------------------------|--------------------------------------|
| `POST`   | `/api/pastes`           | Create a new paste                   |
| `GET`    | `/api/pastes`           | List public pastes (paginated)       |
| `GET`    | `/api/pastes/:id`       | Get paste by ID                      |
| `PUT`    | `/api/pastes/:id`       | Update a paste                       |
| `DELETE` | `/api/pastes/:id`       | Delete a paste                       |
| `GET`    | `/api/pastes/:id/raw`   | Get raw content (text/plain)         |
| `POST`   | `/api/pastes/:id/fork`  | Fork a paste                         |
| `GET`    | `/api/health`           | Health check                         |

### Query Parameters

| Endpoint         | Param     | Description                        |
|------------------|-----------|------------------------------------|
| `GET /api/pastes`| `tag`     | Filter by tag                      |
| `GET /api/pastes`| `page`    | Page number (default: 1)           |
| `GET /api/pastes`| `limit`   | Results per page (default: 50, max: 100) |

### Headers

| Header            | Used on          | Description                           |
|-------------------|------------------|---------------------------------------|
| `X-Requested-With`| POST, PUT, DELETE| Required CSRF token (value: `PasteBox`)|
| `X-Password`      | GET, PUT, DELETE | Password for protected pastes         |

### Examples

```bash
# Create a paste
curl -X POST http://localhost:7777/api/pastes \
  -H 'Content-Type: application/json' \
  -H 'X-Requested-With: PasteBox' \
  -d '{
    "title": "Hello World",
    "content": "print(42)",
    "language": "python",
    "visibility": "public",
    "tags": ["python", "demo"]
  }'

# Create a password-protected, self-destructing paste
curl -X POST http://localhost:7777/api/pastes \
  -H 'Content-Type: application/json' \
  -H 'X-Requested-With: PasteBox' \
  -d '{
    "content": "secret data",
    "password": "mypassword",
    "burn_after_read": true,
    "expires_in": "1h"
  }'

# Get a paste (with password)
curl http://localhost:7777/api/pastes/<id> -H 'X-Password: mypassword'

# List pastes (page 2, 10 per page, filtered by tag)
curl 'http://localhost:7777/api/pastes?tag=python&page=2&limit=10'

# Update a paste
curl -X PUT http://localhost:7777/api/pastes/<id> \
  -H 'Content-Type: application/json' \
  -H 'X-Requested-With: PasteBox' \
  -d '{"title": "Updated Title", "content": "new content"}'

# Fork a paste
curl -X POST http://localhost:7777/api/pastes/<id>/fork \
  -H 'Content-Type: application/json' \
  -H 'X-Requested-With: PasteBox' \
  -d '{"title": "My Fork"}'

# Delete a paste
curl -X DELETE http://localhost:7777/api/pastes/<id> \
  -H 'X-Requested-With: PasteBox'
```

## Testing

```bash
# Backend integration tests (93 tests)
bash backend/test.sh

# E2E flow tests (44 tests)
bash backend/e2e-test.sh

# Frontend unit + component tests (63 tests)
cd frontend && npx vitest run
```

**Total: 200 tests** covering CRUD, security (CSRF, XSS, SQLi, brute-force), pagination, password flows, burn-after-read, visibility, accessibility, and performance.

## Deployment

The app is deployed with:
- **systemd** service (`pastebox.service`)
- **nginx** reverse proxy with SSL (Let's Encrypt / certbot)
- Drogon binds to `127.0.0.1:7777` (not exposed to internet)

## Configuration

Edit `backend/config.json` to change:
- **Listen address** (default: `127.0.0.1`)
- **Port** (default: 7777)
- **Thread count** (default: 4)
- **Max body size** (default: 512K)
- **Log level** (default: INFO)

## License

MIT
