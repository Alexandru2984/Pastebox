#!/usr/bin/env bash
set -euo pipefail
# These tests always launch a disposable server. External URLs are rejected.
PASTEBOX_TEST_EXECUTABLE="${1:-${PASTEBOX_TEST_BINARY:-}}"
if [[ -z "$PASTEBOX_TEST_EXECUTABLE" || "$PASTEBOX_TEST_EXECUTABLE" == *://* ]]; then
  echo 'Usage: bash backend/test.sh /absolute/path/to/test-build/backend' >&2
  exit 2
fi
exec python3 "$(dirname "$0")/security-test.py" --binary "$PASTEBOX_TEST_EXECUTABLE"
