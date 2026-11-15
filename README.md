# PasteBox

A self-hosted home for code snippets, notes, and ideas. C++17 / Drogon API, SQLite, Svelte 5, Vite, and Tailwind CSS.

Live: [pastebox.micutu.com](https://pastebox.micutu.com/)

## What you can do

- Create, edit, delete and fork snippets; copy, share and download them.
- Explore public titles/tags with search, language/tag filters and newest/oldest/most-viewed sorting.
- Keep owned snippets and favorites in a browser-local library; verify/import an owner key on another browser.
- Write with starter templates, text-file import, JSON formatting, Tab indentation, word wrap, UTF-8 byte counts and optional public draft recovery.
- Read with syntax highlighting loaded per language, line numbers, wrapping and font controls. Large snippets avoid expensive highlighting/line-number DOMs.
- Choose public, unlisted or private access, a reading password, expiration, or explicit one-time reveal.
- Embed public, unprotected, reusable snippets with `/embed/ID`.
- Use four themes, keyboard shortcuts and responsive layouts down to 320px.

## Access and privacy

Each new paste returns an **owner key** once. Its digest is stored on the server; the browser saves the key in its local library. This capability is required to edit/delete a paste and to read a private paste. Back it up privately from Library. Clearing browser storage removes that browser's saved keys. An imported key is verified before replacing existing access.

A **read password grants reading only**. Public pastes can be discovered in Explore; unlisted pastes require the link; private pastes require the owner key, including on raw/fork routes. One-time pastes wait for confirmation and are atomically deleted before content is returned. Receiving the content cannot be undone and does not erase copies already made by a reader. Forks inherit protection and expiration; protected provenance is omitted.

Owner keys never appear in share URLs. Read passwords and paste bodies are excluded from the library. Optional drafts store **public, unprotected, reusable** content only; choosing private, unlisted, password or one-time access removes the saved draft. The existing self-hosted Umami integration sends manual homepage/Explore pageviews only, without paste IDs, titles, form values or referrers, and respects Do Not Track/GPC.

Content is plaintext in SQLite. Access control and HTTPS do **not** provide end-to-end encryption. Browser scripts on this origin are trusted, so protect the origin and analytics deployment. No account-based recovery exists.

**Legacy migration:** older pastes have no recorded owner. Their data is retained, but edit/delete fail closed; legacy private records without owner keys cannot be read by guessing their IDs. Recovery requires independently verified operator action. Existing SHA-256 reading passwords continue to verify and upgrade to scrypt on successful reads.

## Build

Requires Node >=22.12 (Node 24 recommended), CMake >=3.20, C++17, Drogon, OpenSSL, SQLite >=3.38 and Brotli development libraries.

```bash
sudo apt install cmake make g++ libdrogon-dev libpq-dev libmariadb-dev \
  libbrotli-dev libhiredis-dev libc-ares-dev libyaml-cpp-dev libsqlite3-dev libssl-dev libjsoncpp-dev uuid-dev zlib1g-dev
npm --prefix frontend ci
cmake -S backend -B /tmp/pastebox-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/pastebox-build -j2
npm --prefix frontend run build -- --outDir /tmp/pastebox-public
```

Preview with disposable data and an automatically selected loopback port:

```bash
python3 scripts/preview.py --binary /tmp/pastebox-build/backend --public /tmp/pastebox-public
```

The preview database is removed when the process exits. For frontend hot reload, `npm --prefix frontend run dev` binds to loopback and proxies `/api` to `127.0.0.1:7777`; point it only at a backend you intend to use. Build outside the production executable/static directory.

## Verification

```bash
ctest --test-dir /tmp/pastebox-build --output-on-failure
npm --prefix frontend test
npm --prefix frontend audit --audit-level=moderate
npm --prefix frontend audit --omit=dev --audit-level=low
cd frontend
npx playwright install chromium
PASTEBOX_TEST_BINARY=/tmp/pastebox-build/backend \
PASTEBOX_TEST_PUBLIC=/tmp/pastebox-public npm run test:e2e
```

The Python API suite always creates its own server/database. It does not accept a remote base URL. `backend/test.sh /path/to/test-build/backend` runs this same isolated suite; the old production-default shell tests have been replaced. Playwright starts another disposable backend on loopback port 19779 and runs desktop/mobile workflows.

Current suites: 74 frontend cases, 19 API security/lifecycle cases and 10 desktop/mobile browser flows. The generated Drogon placeholder test is excluded from the verification count.

GitHub Actions performs clean installs, hardened C++ build, API regressions, frontend tests/build, dependency audit and browser flows. Actions are pinned to commit SHAs; credentials are read-only and there is no deployment or production secret in CI. Dependabot opens weekly dependency update PRs. A workflow file is not evidence that remote CI has already run.

Two low build-only advisories remain in `deepmerge` / the Svelte Vite plugin, with no upstream fix available at review time. There are no moderate/high/critical advisories in the updated npm tree. `npm audit --omit=dev` checks the shipped dependency separately. Re-run audits as advisories change.

## API

Mutations require `X-Requested-With: PasteBox` and JSON bodies where applicable. An Origin, if present, must match the configured origin. Supply owner/read secrets in headers, never query strings.

| Method | Route | Purpose |
| --- | --- | --- |
| POST | `/api/pastes` | Create; returns `owner_token` once |
| GET | `/api/pastes` | Public metadata; `q`, `tag`, `language`, `sort`, `page`, `limit` |
| GET | `/api/pastes/ID` | Read; optional `X-Password` or `X-Owner-Token` |
| GET | `/api/pastes/ID/raw` | Same authorization/burn policy, text/plain |
| GET | `/api/pastes/ID/ownership` | Verify owner key, metadata only; no reveal |
| GET | `/api/pastes/ID/embed` | Public, unprotected, reusable content only |
| PUT | `/api/pastes/ID` | Owner-only title/content/language/visibility/tags update |
| DELETE | `/api/pastes/ID` | Owner-only deletion |
| POST | `/api/pastes/ID/fork` | New owner key; inherited privacy/password/expiry |
| GET | `/api/health` | Database readiness, no private aggregate counts |

Create fields: `content`, `title`, `language`, `visibility`, `password`, `expires_in` (`1h`, `24h`, `7d`, `30d` or empty), `burn_after_read`, `tags`. Content limit: 512 KiB **UTF-8 bytes**; title: 200 bytes; tags: up to 10, each 50 bytes. The 4 MiB transport limit allows JSON escaping overhead. Reading passwords have a compatibility minimum of 4 bytes; use a long, unique passphrase.

A one-time read first returns `409` with `burn_confirmation_required`; explicitly send `X-Confirm-Burn: yes` to reveal. Wrong/missing passwords return a challenge without content; missing-password prompts do not consume the guess budget. `429` includes `Retry-After`. Public listing searches metadata only and retrieves one extra row to determine `has_more` correctly.

## Configuration and operations

`backend/config.json` is a development template. `PASTEBOX_CONFIG` selects an absolute config file; the legacy default is `../config.json` relative to the working directory. SQLite/static paths in that file are relative to the process working directory unless absolute.

- `PASTEBOX_ORIGIN`: allowed browser origin, default `https://pastebox.micutu.com`.
- `PASTEBOX_TRUST_PROXY=1`: opt-in loopback proxy trust. Enable only when nginx overwrites `X-Real-IP`. Forwarded-for chains are ignored.
- Request, write and password budgets have bounded in-memory state and reset on restart. Database growth is capped at 65,536 pages, about 256 MiB with default 4 KiB pages; logs/WAL/backups need separate disk monitoring.
- Production retains the existing dedicated service user and systemd restrictions. This release does not upgrade shared host libraries or modify other applications.

The VPS-specific tested rollout entrypoint is:

```bash
sudo scripts/deploy-vps.sh /path/to/tested/backend /path/to/tested/public
```

It requires the existing PasteBox service, paths, certificates and maintained Cloudflare nginx origin guard. It stores a restricted online SQLite snapshot, checks integrity, performs a disposable restore check, saves old binary/assets/config, validates nginx, installs artifacts and restarts only PasteBox. Old hashed assets remain available to open browser tabs. Failed readiness triggers artifact/config rollback; the live database is retained because schema changes are additive. Backups remain under `/var/backups/pastebox` and require an operator retention/off-host backup policy.

See [security findings and remaining risks](docs/SECURITY-AUDIT.md), [product roadmap](docs/ROADMAP.md), and [release/response procedure](docs/OPERATIONS.md). SEO indexes the homepage only; API/embed responses are noindex and the sitemap contains no user pastes. Hash routing does not provide server-rendered per-paste previews.
