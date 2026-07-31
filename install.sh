#!/usr/bin/env bash
# ╔═══════════════════════════════════════════════════════════════════╗
# ║  ClaVel Framework — Installer                                    ║
# ║  Usage: curl -sS https://clavel.dev/install | bash               ║
# ║         curl -sS https://clavel.dev/install | bash -s -- my-app  ║
# ╚═══════════════════════════════════════════════════════════════════╝

set -euo pipefail

# ── Config ─────────────────────────────────────────────────────────
CLAVEL_VERSION="0.1.0"
CLAVEL_REPO="https://github.com/cbbeandresvargas/clavel"
CLAVEL_ARCHIVE="${CLAVEL_REPO}/archive/refs/tags/v${CLAVEL_VERSION}.tar.gz"
APP_DIR="${1:-my-clavel-app}"

# ── Colors ─────────────────────────────────────────────────────────
if [ -t 1 ]; then
  PINK='\033[35m'; CYAN='\033[36m'; GREEN='\033[32m'
  RED='\033[31m';  DIM='\033[90m';  RESET='\033[0m'
  BOLD='\033[1m'
else
  PINK=''; CYAN=''; GREEN=''; RED=''; DIM=''; RESET=''; BOLD=''
fi

# ── Helpers ─────────────────────────────────────────────────────────
info()    { printf "  ${CYAN}→${RESET} %s\n" "$*"; }
success() { printf "  ${GREEN}✓${RESET} %s\n" "$*"; }
error()   { printf "  ${RED}✗${RESET} %s\n" "$*" >&2; }
die()     { error "$*"; exit 1; }

banner() {
  printf "\n"
  printf "  ${PINK}${BOLD}🌸 ClaVel Framework v${CLAVEL_VERSION}${RESET}\n"
  printf "  ${DIM}────────────────────────────────────${RESET}\n"
  printf "  ${DIM}Laravel's DX. C's performance.${RESET}\n\n"
}

# ── OS detection ────────────────────────────────────────────────────
detect_os() {
  case "$(uname -s)" in
    Linux*)   echo "linux"  ;;
    Darwin*)  echo "macos"  ;;
    MINGW*|MSYS*|CYGWIN*) echo "windows" ;;
    *)        echo "unknown" ;;
  esac
}

# ── Dependency check ─────────────────────────────────────────────────
check_cmd() {
  if ! command -v "$1" &>/dev/null; then
    return 1
  fi
}

check_deps() {
  local os="$1"
  local missing=()

  check_cmd git   || missing+=("git")
  check_cmd cmake || missing+=("cmake")
  check_cmd ninja || check_cmd ninja-build || missing+=("ninja")

  # Check for a C compiler
  if ! check_cmd gcc && ! check_cmd clang && ! check_cmd cc; then
    missing+=("gcc or clang")
  fi

  if [ ${#missing[@]} -gt 0 ]; then
    error "Missing dependencies: ${missing[*]}"
    printf "\n"
    case "$os" in
      linux)
        printf "  Install with:\n"
        printf "    ${DIM}# Ubuntu/Debian${RESET}\n"
        printf "    sudo apt install gcc cmake ninja-build\n\n"
        printf "    ${DIM}# Fedora/RHEL${RESET}\n"
        printf "    sudo dnf install gcc cmake ninja-build\n\n"
        printf "    ${DIM}# Arch${RESET}\n"
        printf "    sudo pacman -S gcc cmake ninja\n\n"
        ;;
      macos)
        printf "  Install with:\n"
        printf "    xcode-select --install\n"
        printf "    brew install cmake ninja\n\n"
        ;;
      windows)
        printf "  In MSYS2 UCRT64:\n"
        printf "    pacman -S mingw-w64-ucrt-x86_64-gcc \\\n"
        printf "              mingw-w64-ucrt-x86_64-cmake \\\n"
        printf "              mingw-w64-ucrt-x86_64-ninja\n\n"
        ;;
    esac
    die "Please install the missing tools and try again."
  fi
}

# ── Download ─────────────────────────────────────────────────────────
download_source() {
  local dest="$1"

  # Try downloading the release tarball first (faster, no git history)
  if check_cmd curl; then
    info "Downloading ClaVel v${CLAVEL_VERSION}..."
    if curl -fsSL "${CLAVEL_ARCHIVE}" -o /tmp/clavel.tar.gz 2>/dev/null; then
      mkdir -p "$dest"
      tar -xzf /tmp/clavel.tar.gz --strip-components=1 -C "$dest"
      rm -f /tmp/clavel.tar.gz
      return 0
    fi
  elif check_cmd wget; then
    info "Downloading ClaVel v${CLAVEL_VERSION}..."
    if wget -qO /tmp/clavel.tar.gz "${CLAVEL_ARCHIVE}" 2>/dev/null; then
      mkdir -p "$dest"
      tar -xzf /tmp/clavel.tar.gz --strip-components=1 -C "$dest"
      rm -f /tmp/clavel.tar.gz
      return 0
    fi
  fi

  # Fallback: clone the repo
  info "Cloning ClaVel from GitHub..."
  git clone --depth 1 --branch "v${CLAVEL_VERSION}" \
    "${CLAVEL_REPO}" "$dest" 2>/dev/null \
  || git clone --depth 1 "${CLAVEL_REPO}" "$dest"
}

# ── Build ─────────────────────────────────────────────────────────────
build_project() {
  local dir="$1"
  local ninja_cmd

  if check_cmd ninja; then
    ninja_cmd="ninja"
  else
    ninja_cmd="ninja-build"
  fi

  info "Configuring CMake..."
  cmake -B "${dir}/cmake-build" -G Ninja -S "$dir" \
    -DCMAKE_BUILD_TYPE=Release \
    >/dev/null 2>&1 \
  || die "CMake configuration failed. Check that gcc and cmake are installed."

  info "Building (this may take a moment)..."
  cmake --build "${dir}/cmake-build" \
    >/dev/null 2>&1 \
  || die "Build failed. Run 'cmake --build cmake-build' manually to see errors."
}

# ── Setup .env ────────────────────────────────────────────────────────
setup_env() {
  local dir="$1"
  if [ -f "${dir}/.env.example" ] && [ ! -f "${dir}/.env" ]; then
    cp "${dir}/.env.example" "${dir}/.env"
    # Generate a random secret key
    local secret
    secret=$(LC_ALL=C tr -dc 'A-Za-z0-9!@#$%^&*' </dev/urandom 2>/dev/null | head -c 32 || echo "change-me-in-production-$(date +%s)")
    # Replace placeholder in .env
    if check_cmd sed; then
      sed -i.bak "s/change-me-in-production/${secret}/g" "${dir}/.env" 2>/dev/null \
        || sed -i '' "s/change-me-in-production/${secret}/g" "${dir}/.env" 2>/dev/null \
        || true
      rm -f "${dir}/.env.bak"
    fi
  fi
}

# ── Main ──────────────────────────────────────────────────────────────
main() {
  banner

  local os
  os=$(detect_os)

  if [ "$os" = "unknown" ]; then
    error "Unsupported OS: $(uname -s)"
    printf "  ClaVel supports Linux, macOS, and Windows (MSYS2).\n\n"
    exit 1
  fi

  # Check if target dir already exists
  if [ -d "$APP_DIR" ]; then
    die "Directory '${APP_DIR}' already exists. Choose a different name:\n  curl -sS https://clavel.dev/install | bash -s -- <name>"
  fi

  info "Target directory: ${BOLD}${APP_DIR}${RESET}"
  info "Checking dependencies..."
  check_deps "$os"
  success "All dependencies found"

  # Download / clone
  download_source "$APP_DIR"
  success "Source ready"

  # Build
  build_project "$APP_DIR"
  success "Build complete"

  # Setup .env
  setup_env "$APP_DIR"
  success "Environment configured"

  # Create storage dirs
  mkdir -p "${APP_DIR}/storage/logs" "${APP_DIR}/storage/public"

  # ── Done ──────────────────────────────────────────────────────────
  local bin="${APP_DIR}/cmake-build/clavel_app"
  [ "$os" = "windows" ] && bin="${bin}.exe"

  printf "\n"
  printf "  ${GREEN}${BOLD}🌸 ClaVel is ready!${RESET}\n\n"
  printf "  ${DIM}────────────────────────────────────${RESET}\n"
  printf "  ${CYAN}Next steps:${RESET}\n\n"
  printf "    cd ${APP_DIR}\n"
  printf "    ${bin}\n\n"
  printf "  Then open ${BOLD}http://localhost:8080${RESET}\n\n"
  printf "  ${DIM}Docs:  https://clavel.dev/docs${RESET}\n"
  printf "  ${DIM}GitHub: ${CLAVEL_REPO}${RESET}\n\n"
}

main "$@"
