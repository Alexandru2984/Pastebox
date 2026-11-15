# PasteBox product plan

Priorities: correctness and privacy first, useful sharing second, discovery/automation last. Each completed stage gets a separate commit without automated attribution trailers. Existing uncommitted local changes are preserved.

## 1. Audit and safe baseline

- Record findings, host boundary, production risks and release/incident procedure.
- Establish tests using disposable data and a separate build directory.

## 2. Access control and data integrity

- Separate owner tokens from read passwords; enforce private access consistently.
- Atomic one-time reads, privacy-preserving forks and additive schema migration.
- Slow versioned password hashing with backwards-compatible verification.
- Strict JSON validation, transactional tags, safe pagination, bounded throttles, no-store API and global headers.
- Security regression tests for forged ownership, raw/fork bypasses and concurrent burns.

## 3. Discovery and personal library

- Server-side title/tag search, language/tag filters, newest/oldest/popular sorting and correct pagination.
- Browser-local library of owned pastes and favorites. Store metadata/capabilities only, never read passwords or private content.
- Explain capability persistence and recovery clearly. Removing a library item must not silently delete a server paste.

## 4. Editor and viewer

- Responsive polished layout, readable mobile controls, keyboard focus and accessible feedback.
- Opt-in local draft recovery (exclude private/password/burn content), file import with size limits, templates and JSON formatting.
- UTF-8 byte and line counts, editing Tab support, wrap/line-number controls and reliable highlighting.
- Copy/share, safe download, owner-only editing and explicit delete confirmation, friendly error/retry states.

## 5. Validation and release

- Run isolated integration tests, existing frontend suite plus meaningful new regressions, desktop/mobile browser flows and production build.
- Snapshot/rollback and deployment smoke checks; describe what is shipped vs still planned.

## 6. SEO and CI (after product/security)

- Canonical homepage, share metadata, favicon, robots/sitemap limited to genuinely public routes; private/user content must not be indexed accidentally.
- CI for clean frontend install, unit tests, production build, dependency audit and isolated C++ API regressions. No production credentials in CI.
- Reproducible local verification, branch dependency updates and documented rollout.

## Next product increments

- Client-side encryption for secret sharing (separate threat-model and browser interoperability work).
- Multi-file pastes and file tabs, version history with owner-only revisions and diff view.
- Expiring share grants and separate revocable read/write capabilities.
- Account-based cross-device collections with passkeys and optional anonymous use.
- Moderation/reporting queue, operator-only takedowns and retention policies before wider promotion.
- FTS search after dataset profiling, cursor pagination, per-user/API quotas and scoped API keys.
- Sandboxed Markdown preview (no raw HTML), accessible code permalink lines and QR sharing.
- Opt-in self-hosted aggregate analytics without paste IDs/content or private routes.
- Automated encrypted off-host backups with restore drills; alerting on disk, errors, rate spikes and backup freshness.
- Internationalization, share cards for public pastes, public collections and curated snippets.

Avoid launching comments, executable code previews, arbitrary URL imports or anonymous HTML rendering without dedicated abuse and sandbox controls.

## Delivered product stage

Explore now searches titles/tags and filters language/tag with stable newest/oldest/popular ordering. Browser library supports favorites, local owner capabilities and verified key restoration without revealing one-time content. New responsive application shell, native accessible theme picker, error retry states and semantic cards replace nested controls.

The composer adds starter templates, text/code file import, JSON formatting, UTF-8 byte validation, guarded submission, word wrap, Tab indentation with an Escape exit, and explicit public-only draft persistence. Protected/unlisted/one-time content is excluded from draft storage. No cross-device account or end-to-end encryption is claimed.

## Delivered viewer and browser validation stage

Viewer includes immediate visible content, lazy per-language syntax highlighting, wrapping/line-number/font-size controls, browser favorites, safe filename downloads, owner-only edit/delete and a delete confirmation dialog. Private key restoration verifies against a metadata-only ownership endpoint. Public-only embed has a dedicated API which refuses private, unlisted, password-protected and one-time content even when an owner key is supplied. Public embed documents alone allow external framing.

Initial JS reduced from about 1,011 KB / 341 KB gzip to 125 KB / 47 KB gzip; language chunks load on demand. Plain text/large payloads skip expensive automatic highlighting and large line counts do not create one DOM node per line. Desktop and mobile browser flows cover CRUD, actual highlighted content, privacy, owner recovery, one-time reveal, draft exclusion, file import, JSON formatting, themes and 320px layout fit.

## Delivered SEO, CI and operational preparation

Homepage canonical/social card, app-owned robots/sitemap, API/embed noindex and manual public-only analytics are implemented. CI pins Actions SHAs, uses read-only checkout credentials and repeats clean frontend install, audits/build/tests plus isolated C++ and desktop/mobile workflows. Weekly dependency PRs are configured. The VPS rollout includes a restricted online database snapshot, restore verification and automatic artifact rollback. Nginx sanitizes visitor identity, rejects direct non-Cloudflare peers, bounds API ingress and supports only the dedicated public embed page for external framing. C++ connection/pipeline limits and release hardening are configured.

Pending wider product work remains listed above: cross-device accounts, encryption, multi-file/version history, abuse moderation, external security/accessibility review, alerting and encrypted off-host recovery. These are not claimed as shipped by this release.

## Production status

This release is live. See [release verification and residual work](RELEASE-2026-11-15.md). The checked-in roadmap distinguishes delivered functionality from the next product/security increments; remote CI still requires a push.

## Next release in progress: revision history and safe reading

1. Owner-only bounded revision history, comparison and restore; content edits require an exact current revision so two tabs cannot silently overwrite one another.
2. Historical snapshots contain title/content/language/tags only. Access controls, passwords and expiration remain current; one-time pastes never create reusable snapshots. Deletion/expiration removes history through the same foreign-key lifecycle.
3. Lazy, sandboxed Markdown preview without raw HTML or remote image requests, with browser XSS/privacy regressions.
4. Repeat isolated API/frontend/browser checks, back up and deploy the tested artifacts, then verify the public site read-only.

No cross-device account, off-host backup destination or remote push is assumed by this increment.

### Revision stage delivered

Implemented owner-only bounded history, line comparison with a large-file fallback, restore as a new head, explicit history clearing and optimistic edit preconditions. Current privacy/expiry are preserved on restore, one-time pastes remain snapshot-free, and stale editor content remains available after conflicts. API and browser regressions run only on disposable data.

### Markdown stage delivered

Drafts and saved Markdown now have an on-demand preview with worker deadlines, a strict sanitized HTML subset, isolated framing and hidden remote images. Private reading content/owner keys remain in separate origins; preview cannot read parent storage. The full source is retained on every preview error. New browser checks cover preview without publishing, hostile input, worker/sanitizer loading on demand, theme changes and narrow layouts.

The expanded app entry is about 143 KB / 53 KB gzip; the 50 KB parser worker and 30 KB sanitizer chunk load only when preview is opened. Accounts, encryption, multi-file documents, moderation and off-host recovery remain separate future work.

### Password resource guard delivered

Added an aggregate scrypt budget alongside per-IP limits, with an explicit retry response. Distributed password traffic cannot make derivation run without a process-wide cap; owner-key reads do not consume this budget. Regression uses disposable clients/database and verifies readiness remains available.
