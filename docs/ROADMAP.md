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
