#!/usr/bin/env bash
# Build & deploy libsie_jll for a tagged upstream release.
#
# Prompts for a libsie-z version, resolves the matching commit SHA on
# GitHub (no clone needed), and runs build_tarballs.jl with the right
# environment variables. Any extra arguments are forwarded to Julia
# (e.g. a single platform triple for a one-off test build, or
# `--deploy=local` to skip the GitHub release upload).
#
# Usage:
#   ./build_and_deploy.sh                   # full multi-platform deploy
#   ./build_and_deploy.sh x86_64-linux-gnu  # single-platform test build
#   LIBSIE_VERSION=0.3.0 ./build_and_deploy.sh --verbose --debug
#
# Env overrides:
#   LIBSIE_VERSION   skip the interactive prompt
#   LIBSIE_REPO      upstream git repo (default: efollman/libsie-z)
#   JLL_DEPLOY       deploy target for build_tarballs.jl
#                    (default: efollman/libsie_jll.jl)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

REPO="${LIBSIE_REPO:-https://github.com/efollman/libsie-z.git}"
DEPLOY="${JLL_DEPLOY:-efollman/libsie_jll.jl}"

# 1. Version ----------------------------------------------------------------
VERSION="${LIBSIE_VERSION:-}"
if [ -z "$VERSION" ]; then
    read -rp "libsie-z version to build (e.g. 0.3.0): " VERSION
fi
VERSION="${VERSION#v}"   # strip a leading "v" if the user typed one
if [ -z "$VERSION" ]; then
    echo "error: no version provided" >&2
    exit 1
fi
TAG="v${VERSION}"

# 2. Resolve commit SHA for the tag without cloning the repo ----------------
# `git ls-remote` only fetches the refs list. We ask for the dereferenced
# tag (`^{}`) first so annotated tags resolve to the commit they point at;
# fall back to the bare ref for lightweight tags.
echo "Resolving $TAG against $REPO ..." >&2
LSREMOTE="$(git ls-remote "$REPO" "refs/tags/${TAG}^{}" "refs/tags/${TAG}" || true)"
if [ -z "$LSREMOTE" ]; then
    echo "error: tag $TAG not found on $REPO" >&2
    exit 1
fi
# Prefer the dereferenced line if present.
HASH="$(echo "$LSREMOTE" | awk '/\^\{\}$/ {print $1; found=1; exit} END {exit !found}')" \
    || HASH="$(echo "$LSREMOTE" | awk 'NR==1 {print $1}')"
if [ -z "$HASH" ]; then
    echo "error: failed to extract commit SHA for $TAG" >&2
    exit 1
fi
echo "  -> $HASH" >&2

# 3. Build args -------------------------------------------------------------
# When no extra args are supplied, do a full multi-platform deploy.
# Otherwise pass the user's args straight through (test builds, --debug, …).
if [ "$#" -eq 0 ]; then
    set -- --verbose --deploy="$DEPLOY"
fi

# 4. Hand off to BinaryBuilder ---------------------------------------------
exec env \
    LIBSIE_VERSION="$VERSION" \
    LIBSIE_TREE_HASH="$HASH" \
    LIBSIE_REPO="$REPO" \
    BINARYBUILDER_AUTOMATIC_APPLE=true \
    julia --project=. build_tarballs.jl "$@"
