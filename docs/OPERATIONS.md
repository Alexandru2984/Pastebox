# PasteBox operations

## Before release

1. Build artifacts outside `backend/build` and run CTest, frontend unit tests, dependency audit and desktop/mobile browser flows against those exact artifacts.
2. Inspect `git diff`, preserve local work and commit the completed stage under the repository owner's existing identity. Do not add attribution trailers.
3. `scripts/deploy-vps.sh` is deliberately specific to this VPS. Review paths, cloudflare origin guard, certificate and systemd bindings before use. Do not run it against an unrelated host.

## Rollout and recovery

The rollout creates `/var/backups/pastebox/release-TIMESTAMP-SUFFIX` with mode 0700; the database snapshot has mode 0600. The online SQLite backup API includes committed WAL data. The script checks integrity and restores into a second disposable file before touching the active artifact. This is a local restore drill, not an off-host disaster-recovery exercise.

On command failure after staging, the script restores the previous binary, static tree, app config, nginx vhost and app environment drop-in, restarts PasteBox, then validates/reloads nginx. Schema additions are compatible with the previous artifact. It does not overwrite the live database with an earlier snapshot, which would discard new user writes. A rollback to the previous application reintroduces its authorization weaknesses: contain access immediately if a security rollback is needed.

For later manual rollback: stop PasteBox, install the saved binary and public tree back to the exact service paths, restore configs/drop-in, run `nginx -t`, `systemctl daemon-reload`, restart PasteBox, reload nginx and verify readiness. Keep the current database. Database restoration is a separate incident-recovery action requiring a maintenance window and explicit evaluation of new writes; preserve the current DB/WAL before any restore.

## Smoke checks

- `systemctl is-active pastebox` and loopback `/api/health` return healthy; the public HTTPS endpoint also returns `{"status":"ok"}`.
- Root serves the new HTML, built asset URLs return 200 with immutable caching, and `/robots.txt` + `/sitemap.xml` describe this application.
- API errors are JSON with `Cache-Control: no-store`, `X-Content-Type-Options: nosniff`, `X-Robots-Tag: noindex...` and a strict CSP.
- Embed documents allow framing; their API returns only public, unprotected, reusable content. Other pages stay same-origin framed.
- Do not use customer IDs for authorization/burn tests. Run destructive/concurrency checks only against the disposable harness.
- Shared nginx validates as a whole; only the PasteBox vhost is changed. No Cloudflare tunnel or other application's configuration needs to change.

## Security incident exercise

Scenarios exercised locally: unknown owner attempts edit/delete/private access (refused); concurrent secret delivery over raw/JSON (one success); protection-preserving fork; failed key restoration preserves existing key. The release script additionally exercises snapshot integrity and local restoration. These automated checks do not replace an operator tabletop or off-host restore drill.

If a vulnerability or token disclosure is suspected, restrict the affected routes, preserve a restricted snapshot and relevant time window, identify affected IDs without dumping content, revoke/recover capabilities only after ownership verification, assess scope, remediate and verify before reopening. Never publish body/password/key values in tickets or logs. Incident owner: VPS operator. Notification obligations, contacts, costs and response targets require operator confirmation.

## Still needed before broad promotion

- Encrypted off-host backup schedule, retention policy and restore drill on another host.
- Alerts on service restarts, 5xx/429 spikes, disk/database/WAL growth and backup age; current MTTD is unmeasured.
- Public content reporting, operator moderation and retention/abuse policy.
- Independently verified recovery for legacy records; do not infer ownership from a shared link or reading password.
- Review/update shared Drogon (host reports 1.9.11), OpenSSL, nginx and Cloudflare software in a separate host maintenance window. The application binds parameters and does not use the ORM JSON Criteria constructor.
- External penetration test, accessibility audit and operator tabletop. No exhaustive security/compliance certification is claimed.
