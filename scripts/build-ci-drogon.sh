#!/usr/bin/env bash
# Build the production framework version into an isolated CI prefix.
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo 'Usage: scripts/build-ci-drogon.sh /tmp/new-dependency-prefix' >&2
  exit 2
fi
PASTEBOX_DEP_PREFIX="$(realpath -m -- "$1")"
if [[ "$PASTEBOX_DEP_PREFIX" != /tmp/* || -e "$PASTEBOX_DEP_PREFIX" ]]; then
  echo 'Use a new, nonexistent prefix below /tmp. No host libraries are replaced.' >&2
  exit 2
fi
PASTEBOX_DROGON_SHA=a22956b82b6b221ceeff83913c3014ce0d048555
PASTEBOX_TRANTOR_SHA=43fd79b2dbac59608a819ebba167e8fe2c079d90
PASTEBOX_DEP_WORK="$(mktemp -d /tmp/pastebox-drogon-source-XXXXXX)"
trap 'rm -rf -- "$PASTEBOX_DEP_WORK"' EXIT
git init --quiet "$PASTEBOX_DEP_WORK/source"
git -C "$PASTEBOX_DEP_WORK/source" remote add origin https://github.com/drogonframework/drogon.git
git -C "$PASTEBOX_DEP_WORK/source" fetch --quiet --depth 1 origin "$PASTEBOX_DROGON_SHA"
git -C "$PASTEBOX_DEP_WORK/source" checkout --quiet --detach "$PASTEBOX_DROGON_SHA"
git -C "$PASTEBOX_DEP_WORK/source" -c protocol.file.allow=never submodule update --init --depth 1 trantor
[[ "$(git -C "$PASTEBOX_DEP_WORK/source" rev-parse HEAD)" == "$PASTEBOX_DROGON_SHA" ]]
[[ "$(git -C "$PASTEBOX_DEP_WORK/source/trantor" rev-parse HEAD)" == "$PASTEBOX_TRANTOR_SHA" ]]
cmake -S "$PASTEBOX_DEP_WORK/source" -B "$PASTEBOX_DEP_WORK/build" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PASTEBOX_DEP_PREFIX" \
  -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_CXX_STANDARD=17 \
  -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF -DBUILD_CTL=OFF \
  -DBUILD_SHARED_LIBS=OFF -DBUILD_ORM=ON -DBUILD_SQLITE=ON \
  -DBUILD_POSTGRESQL=OFF -DBUILD_MYSQL=OFF -DBUILD_REDIS=OFF
cmake --build "$PASTEBOX_DEP_WORK/build" --parallel 2
cmake --install "$PASTEBOX_DEP_WORK/build"
printf 'Drogon 1.9.11 (%s), Trantor %s installed in %s\n' \
  "$PASTEBOX_DROGON_SHA" "$PASTEBOX_TRANTOR_SHA" "$PASTEBOX_DEP_PREFIX"
