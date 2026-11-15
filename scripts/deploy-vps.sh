#!/usr/bin/env bash
# VPS-specific rollout. Run only after unit/API/browser checks on these artifacts.
set -euo pipefail
if [[ "$EUID" -ne 0 || $# -ne 2 ]]; then
  echo 'Usage: sudo scripts/deploy-vps.sh /path/to/tested/backend /path/to/tested/public' >&2
  exit 2
fi
PASTEBOX_REPO="$(cd -- "$(dirname -- "$0")/.." && pwd)"
PASTEBOX_BINARY="$(realpath -- "$1")"
PASTEBOX_PUBLIC="$(realpath -- "$2")"
PASTEBOX_LIVE="$PASTEBOX_REPO/backend/build"
[[ -x "$PASTEBOX_BINARY" && -f "$PASTEBOX_PUBLIC/index.html" ]]
[[ "$PASTEBOX_BINARY" != "$PASTEBOX_LIVE/backend" && "$PASTEBOX_PUBLIC" != "$PASTEBOX_LIVE/public" ]]
[[ "$(systemctl show pastebox -p WorkingDirectory --value)" == /var/lib/pastebox/runtime ]]
exec 9>/run/lock/pastebox-deploy.lock
flock -n 9
install -d -m 0700 /var/backups/pastebox
PASTEBOX_RELEASE_TIME="$(date -u +%Y%m%dT%H%M%SZ)"
PASTEBOX_BACKUP="$(mktemp -d "/var/backups/pastebox/release-$PASTEBOX_RELEASE_TIME-XXXXXX")"
chmod 0700 "$PASTEBOX_BACKUP"
cp -a "$PASTEBOX_LIVE/backend" "$PASTEBOX_BACKUP/backend"
cp -a "$PASTEBOX_LIVE/public" "$PASTEBOX_BACKUP/public"
cp -a /etc/pastebox/config.json "$PASTEBOX_BACKUP/config.json"
cp -a /etc/nginx/sites-available/pastebox.micutu.com "$PASTEBOX_BACKUP/nginx.conf"
if [[ -f /etc/systemd/system/pastebox.service.d/10-app.conf ]]; then
  cp -a /etc/systemd/system/pastebox.service.d/10-app.conf "$PASTEBOX_BACKUP/10-app.conf"
fi
python3 - "$PASTEBOX_BACKUP" <<'PY'
import os, sqlite3, sys
from pathlib import Path
root = Path(sys.argv[1])
with sqlite3.connect('file:/var/lib/pastebox/runtime/pastebox.db?mode=ro',uri=True) as source:
    with sqlite3.connect(root/'pastebox.db') as destination:
        source.backup(destination)
        assert destination.execute('PRAGMA integrity_check').fetchone()[0] == 'ok'
os.chmod(root/'pastebox.db',0o600)
# Restore drill on a separate disposable database, without reading paste contents.
with sqlite3.connect(root/'pastebox.db') as snapshot:
    with sqlite3.connect(root/'restore-check.db') as restored:
        snapshot.backup(restored)
        assert restored.execute('PRAGMA integrity_check').fetchone()[0] == 'ok'
(root/'restore-check.db').unlink()
PY
rollback() {
  trap - ERR
  echo "Rollout failed. Restoring artifacts from $PASTEBOX_BACKUP" >&2
  install -m 0755 "$PASTEBOX_BACKUP/backend" "$PASTEBOX_LIVE/backend.rollback"
  mv -f "$PASTEBOX_LIVE/backend.rollback" "$PASTEBOX_LIVE/backend"
  if [[ -d "$PASTEBOX_LIVE/public" ]]; then mv "$PASTEBOX_LIVE/public" "$PASTEBOX_BACKUP/failed-public"; fi
  cp -a "$PASTEBOX_BACKUP/public" "$PASTEBOX_LIVE/public"
  cp -a "$PASTEBOX_BACKUP/config.json" /etc/pastebox/config.json
  cp -a "$PASTEBOX_BACKUP/nginx.conf" /etc/nginx/sites-available/pastebox.micutu.com
  if [[ -f "$PASTEBOX_BACKUP/10-app.conf" ]]; then
    cp -a "$PASTEBOX_BACKUP/10-app.conf" /etc/systemd/system/pastebox.service.d/10-app.conf
  else
    rm -f /etc/systemd/system/pastebox.service.d/10-app.conf
  fi
  systemctl daemon-reload
  systemctl restart pastebox
  nginx -t && systemctl reload nginx
  echo 'Database was kept: migrations are additive, so new customer writes are not discarded.' >&2
  exit 1
}
trap rollback ERR
install -m 0755 "$PASTEBOX_BINARY" "$PASTEBOX_LIVE/backend.next"
# Build the replacement assets in a sibling directory for a same-filesystem rename.
PASTEBOX_NEXT="$(mktemp -d "$PASTEBOX_LIVE/public-next-XXXXXX")"
cp -a "$PASTEBOX_PUBLIC/." "$PASTEBOX_NEXT/"
if [[ -d "$PASTEBOX_LIVE/public/assets" ]]; then
  mkdir -p "$PASTEBOX_NEXT/assets"
  cp --archive --update=none "$PASTEBOX_LIVE/public/assets/." "$PASTEBOX_NEXT/assets/"
fi
chmod 0755 "$PASTEBOX_NEXT"
install -m 0644 "$PASTEBOX_REPO/backend/config.json" /etc/pastebox/config.json
install -d -m 0755 /etc/systemd/system/pastebox.service.d
install -m 0644 "$PASTEBOX_REPO/deploy/pastebox.env.conf" /etc/systemd/system/pastebox.service.d/10-app.conf
install -m 0644 "$PASTEBOX_REPO/deploy/pastebox.nginx.conf" /etc/nginx/sites-available/pastebox.micutu.com
nginx -t
mv -f "$PASTEBOX_LIVE/backend.next" "$PASTEBOX_LIVE/backend"
mv "$PASTEBOX_LIVE/public" "$PASTEBOX_BACKUP/public-live-inode"
mv "$PASTEBOX_NEXT" "$PASTEBOX_LIVE/public"
systemctl daemon-reload
systemctl restart pastebox
for PASTEBOX_ATTEMPT in {1..20}; do
  if curl --fail --silent --max-time 2 http://127.0.0.1:7777/api/health > /dev/null; then break; fi
  printf 'Waiting for readiness (%s/20)\n' "$PASTEBOX_ATTEMPT" >&2
  sleep 0.5
done
curl --fail --silent --max-time 5 http://127.0.0.1:7777/api/health
systemctl reload nginx
curl --fail --silent --max-time 10 --resolve pastebox.micutu.com:443:127.0.0.1 https://pastebox.micutu.com/api/health
curl --fail --silent --max-time 10 --resolve pastebox.micutu.com:443:127.0.0.1 https://pastebox.micutu.com/robots.txt > /dev/null
printf '\nRelease installed. Restricted backup and restore-tested snapshot: %s\n' "$PASTEBOX_BACKUP"
trap - ERR
