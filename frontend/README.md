# PasteBox frontend

Svelte 5, Vite and Tailwind. See the [project README](../README.md) for ownership/privacy semantics, the disposable backend preview, build prerequisites and deployment.

```bash
npm ci
npm test
npm run build
npm run dev
```

Development/preview bind to loopback. The dev server forwards `/api` to `127.0.0.1:7777`; use a disposable backend when testing changes.

Browser flows use `npm run test:e2e`; set `PASTEBOX_TEST_BINARY` and `PASTEBOX_TEST_PUBLIC` to the separately built C++ binary and frontend artifact. Playwright creates its own disposable database and backend. Install the browser first with `npx playwright install chromium`.

The browser library stores owner keys and metadata locally. Do not add password/content persistence to it. Optional public drafts must continue excluding private, unlisted, password-protected and one-time content. API content is always displayed as text; syntax markup comes only from highlight.js. External embedding uses the restricted public embed API.
