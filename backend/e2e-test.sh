#!/usr/bin/env bash
set -euo pipefail
# API lifecycle/security suite; browser workflows: npm --prefix frontend run test:e2e.
exec bash "$(dirname "$0")/test.sh" "$@"
