# PasteBox security review

Date: 2026-11-15. Scope: tracked source, dependency audit, read-only inspection of the PasteBox systemd service and nginx configuration. No customer content was inspected. This is a source and targeted runtime review, not a claim of exhaustive penetration testing.

## Findings at baseline

| Priority | Finding | Impact | Remediation |
| --- | --- | --- | --- |
| Critical | PUT/DELETE authorize by optional reading password only | Anyone knowing an unprotected paste ID can overwrite/delete it; readers with passwords also get write access | Separate random owner capability, store its digest only; deny legacy mutations without proven ownership |
| High | Private is only a list filter | Anyone with the ID reads a private paste | Enforce owner capability on all private read paths |
| High | Raw does not consume burn pastes; GET deletes asynchronously | Repeated/raw/concurrent disclosure of a one-time secret | Atomic conditional DELETE with RETURNING before returning content; same behavior on raw; disallow burning through fork |
| High | Fork always creates public content | Private/password content and provenance exposed | Inherit visibility/password/expiry, issue separate owner capability; omit private parent links |
| High | Salted SHA-256 is a fast password hash | Offline password guessing after database theft | Versioned scrypt; verify and upgrade existing SHA-256 hashes on successful reads |
| High | Arbitrary JSON types reach asString/asBool | Exceptions and unstable error behavior | Validate objects, fields, tags, enums and byte lengths before use |
| Medium | Limits use TCP peer behind nginx | One visitor can rate-limit/lock out everyone | Explicit trusted-proxy mode, sanitized X-Real-IP only from loopback; bounded state |
| Medium | Tags update counter can never finish; nontransactional tag writes | Hung requests, partial updates, inconsistent tags | Validate and deduplicate; transactional create/update/fork |
| Medium | Private responses/cache and API errors have inconsistent headers | Sensitive content retention; middleware misses protections | Central API no-store and global security headers |
| Medium | Pagination multiplication uses signed int | Overflow and unbounded offsets | Strict bounded integer parsing, deterministic ordering, fetch one extra row |
| Medium | RNG return values ignored; short public IDs | Failure not detected, smaller discovery space | Check OpenSSL results; longer random IDs |
| Medium | Password prompt is counted as a failed guess | Visitors browsing locked pastes can trigger lockout | Challenge without a supplied password does not increment failures |
| Medium | No storage quota or content-abuse process | Disk exhaustion, harmful public uploads | Storage and creation budgets; document moderation/retention work |
| Low | Health check counts all private records | Unneeded aggregate disclosure and growing query work | SELECT 1 readiness, no private counts |
| Low | Wrong development proxy port (3000 vs 7777) | Development calls another local application | Correct and bind dev/preview to loopback |
| Low | Nested buttons, stale highlight lifecycle, broken protected raw links | Accessibility and content display failures | Semantic cards, lifecycle-safe highlighting, credentialed raw retrieval |

Dependency results must be rechecked from the lockfile after updates; build-only advisories must not be described as demonstrated production exploits. At baseline, 63 frontend tests passed; several cover rendering only. The old shell suites default to the production port and assume anonymous write access: do not run them on production.

## Threat model (ciso-review)

Top risks: tampering via missing object ownership (high likelihood/high impact); information disclosure through private, raw, burn and fork routes (high/high); resource exhaustion through expensive password checks and unbounded writes (medium/high). A database compromise exposes plaintext pastes and metadata, password verifiers and owner-token digests. TLS/password access control does not provide end-to-end encryption. User counts, financial loss, ALE, contractual duties and vendor DPAs are unknown and are not estimated without evidence.

Boundaries: browser -> Cloudflare -> origin nginx or tunnel ingress -> loopback Drogon -> SQLite. Cloudflare headers are trusted only at an explicitly configured proxy boundary. Other local VPS services remain part of the host threat model. Existing systemd isolation is strong (dedicated user, strict filesystem, no capabilities, loopback-only network, 128 MiB memory ceiling). The inspected nginx vhost is in sites-available; that alone does not prove it is serving requests. Trace actual public headers before rollout.

## Release and response

1. Run regression tests against a disposable SQLite database on a separate loopback port. Do not scan or mutate customer pastes.
2. Build outside the live executable/static directory. Back up SQLite with its online backup API and back up the exact active binary/assets/config before an authorized rollout. Never copy a WAL database as a single file while it is open.
3. Deploy a tested artifact, restart only pastebox, verify readiness, public headers and static assets. On failure restore the previous artifact; migrations must be additive so the old artifact can still read the database.
4. Legacy ownership cannot be reconstructed. Legacy pastes remain stored; writes fail closed. Legacy private records lacking ownership cannot be made readable to everyone as a migration shortcut. Operator recovery needs an independently verified owner.
5. If unauthorized access is suspected: preserve a restricted snapshot and time window, restrict write/affected routes, rotate exposed secrets, review access/error logs without publishing paste bodies, assess scope, restore service and document the incident. Do not log passwords or owner tokens. Owner: VPS operator. A real tabletop exercise and verified off-host restore remain pending.
6. Detection to implement: alert on repeated 403/429/5xx, service restarts, disk/WAL growth and backup age. Suggested target: notify the operator within 15 minutes. Current MTTD is unmeasured; logs alone do not establish detection.
7. Cloudflare, hosting provider and existing analytics are external dependencies. No new vendor is required for this release. Regulatory applicability and notification obligations need operator/legal assessment; this audit does not establish compliance.

References: [OWASP object authorization](https://owasp.org/API-Security/editions/2023/en/0xa1-broken-object-level-authorization/), [OWASP password storage](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html). Review method: [ciso-review skill](/home/micu/.codex/skills/ciso-review/SKILL.md).
