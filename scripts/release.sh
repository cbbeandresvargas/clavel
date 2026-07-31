#!/usr/bin/env bash
# scripts/release.sh — Crear un release de ClaVel
#
# Uso: ./scripts/release.sh <versión>
#      ./scripts/release.sh 0.2.0
#
# El script:
#   1. Valida que no haya cambios sin commitear
#   2. Crea el tag vX.Y.Z firmado
#   3. Lo sube a origin (GitHub Actions toma el control desde ahí)

set -euo pipefail

VERSION="${1:-}"

# ── Colors ─────────────────────────────────────────────────────────
PINK='\033[35m'; CYAN='\033[36m'; GREEN='\033[32m'
RED='\033[31m';  DIM='\033[90m';  RESET='\033[0m'; BOLD='\033[1m'

info()    { printf "  ${CYAN}→${RESET} %s\n" "$*"; }
success() { printf "  ${GREEN}✓${RESET} %s\n" "$*"; }
error()   { printf "  ${RED}✗${RESET} %s\n" "$*" >&2; }
die()     { error "$*"; exit 1; }

# ── Validate input ──────────────────────────────────────────────────
if [ -z "$VERSION" ]; then
  printf "Usage: %s <version>\n" "$0"
  printf "Example: %s 0.2.0\n" "$0"
  exit 1
fi

# Strip leading 'v' if provided
VERSION="${VERSION#v}"
TAG="v${VERSION}"

printf "\n  ${PINK}${BOLD}🌸 ClaVel Release Tool${RESET}\n"
printf "  ${DIM}────────────────────────────────${RESET}\n\n"

# ── Must be on main ─────────────────────────────────────────────────
BRANCH=$(git rev-parse --abbrev-ref HEAD)
if [ "$BRANCH" != "main" ]; then
  die "Releases must be created from 'main'. Current branch: ${BRANCH}"
fi
success "On branch main"

# ── No uncommitted changes ──────────────────────────────────────────
if ! git diff --quiet || ! git diff --cached --quiet; then
  die "There are uncommitted changes. Commit or stash them first."
fi
success "Working tree clean"

# ── Tag doesn't already exist ───────────────────────────────────────
if git tag -l "$TAG" | grep -q "$TAG"; then
  die "Tag ${TAG} already exists. Did you mean a different version?"
fi

# ── Verify CHANGELOG has an entry for this version ─────────────────
if ! grep -q "\[${VERSION}\]" CHANGELOG.md 2>/dev/null; then
  die "CHANGELOG.md has no entry for [${VERSION}]. Add it before releasing."
fi
success "CHANGELOG.md entry found"

# ── Build check (optional but recommended) ─────────────────────────
if command -v cmake &>/dev/null && command -v ninja &>/dev/null; then
  info "Running build check..."
  cmake -B cmake-build -G Ninja -S . -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1
  cmake --build cmake-build >/dev/null 2>&1
  success "Build check passed"
else
  printf "  ${DIM}⚠  cmake/ninja not found locally — skipping build check${RESET}\n"
fi

# ── Extract release notes from CHANGELOG ───────────────────────────
info "Extracting release notes from CHANGELOG.md..."
# Grab everything between ## [VERSION] and the next ## [
NOTES=$(awk "/^## \[${VERSION}\]/,/^## \[/" CHANGELOG.md \
  | sed '1d;$d' \
  | sed '/^[[:space:]]*$/{ N; /^\n[[:space:]]*$/d }')

if [ -z "$NOTES" ]; then
  NOTES="Release ${TAG} — see CHANGELOG.md for details."
fi

# Write notes to temp file
NOTES_FILE=$(mktemp /tmp/clavel-release-notes.XXXXXX)
printf "%s" "$NOTES" > "$NOTES_FILE"
trap 'rm -f "$NOTES_FILE"' EXIT

# ── Create and push tag ─────────────────────────────────────────────
info "Creating tag ${TAG}..."
git tag -a "$TAG" -F "$NOTES_FILE"
success "Tag ${TAG} created"

info "Pushing tag to origin..."
git push origin "$TAG"
success "Tag pushed — GitHub Actions will build and publish the release"

printf "\n"
printf "  ${GREEN}${BOLD}✓ Release ${TAG} initiated!${RESET}\n\n"
printf "  Watch the workflow at:\n"
printf "  ${DIM}https://github.com/cbbeandresvargas/clavel/actions${RESET}\n\n"
printf "  The release will appear at:\n"
printf "  ${DIM}https://github.com/cbbeandresvargas/clavel/releases/tag/${TAG}${RESET}\n\n"
