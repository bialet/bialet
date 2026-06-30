#!/bin/sh
# Bialet installer — curl -sSL https://get.bialet.dev | sh
set -e

# ── Configuration ──────────────────────────────────────────────
BIN_NAME="bialet"
REPO="bialet/bialet"
INSTALL_DIR="${BIN_DIR:-$HOME/.local/bin}"

# ── Helpers ────────────────────────────────────────────────────
bold()   { printf "\033[1m%s\033[0m\n" "$*"; }
green()  { printf "\033[32m%s\033[0m\n" "$*"; }
yellow() { printf "\033[33m%s\033[0m\n" "$*"; }
red()    { printf "\033[31m%s\033[0m\n" "$*" >&2; }
abort()  { red "$1"; exit 1; }

# ── Detect platform ────────────────────────────────────────────
OS="$(uname -s)"
ARCH="$(uname -m)"

case "$OS" in
  Linux)  PLATFORM="linux" ;;
  Darwin) PLATFORM="macos" ;;
  *)       abort "Unsupported OS: $OS" ;;
esac

case "$ARCH" in
  x86_64|amd64)  ARCH_NORM="x86_64" ;;
  aarch64|arm64) ARCH_NORM="arm64" ;;
  *)              abort "Unsupported architecture: $ARCH" ;;
esac

# ── Check dependencies ─────────────────────────────────────────
require() {
  command -v "$1" >/dev/null 2>&1 || abort "$1 is required but not installed."
}
require curl
require unzip
require mktemp

# ── Runtime deps warning ───────────────────────────────────────
check_runtime_deps() {
  local missing=""
  if [ "$PLATFORM" = "linux" ]; then
    command -v ldconfig >/dev/null 2>&1 || return
    ldconfig -p 2>/dev/null | grep -q libsqlite3 || missing="libsqlite3"
    ldconfig -p 2>/dev/null | grep -q libcurl   || missing="$missing libcurl"
  elif [ "$PLATFORM" = "macos" ]; then
    # macOS usually ships sqlite3; curl is system
    true
  fi
  if [ -n "$missing" ]; then
    yellow "Note: runtime libraries may be needed: $missing"
    yellow "See https://bialet.dev/installation.html for distro-specific packages."
  fi
}

# ── Fetch latest release tag ───────────────────────────────────
echo "==> Fetching latest Bialet release..."
RELEASE_URL="https://api.github.com/repos/$REPO/releases/latest"
TAG=$(curl -sSLf "$RELEASE_URL" 2>/dev/null | grep '"tag_name"' | head -1 | sed 's/.*"tag_name": *"\([^"]*\)".*/\1/')

if [ -z "$TAG" ]; then
  yellow "Could not determine latest release. Using default version."
  TAG="v0.10.0"
fi
green "==> Latest version: ${TAG#v}"

# ── Download ───────────────────────────────────────────────────
ZIPNAME="${BIN_NAME}-${TAG}-${PLATFORM}-${ARCH_NORM}.zip"
DOWNLOAD_URL="https://github.com/$REPO/releases/download/$TAG/$ZIPNAME"

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

echo "==> Downloading $ZIPNAME..."
curl -sSLf "$DOWNLOAD_URL" -o "$TMPDIR/$ZIPNAME" || abort "Download failed. Check that release $TAG exists."

# ── Verify zip ──────────────────────────────────────────────────
if ! unzip -tq "$TMPDIR/$ZIPNAME" > /dev/null 2>&1; then
  abort "Downloaded file is not a valid zip. Release asset may be missing for $TAG."
fi

# ── Install ────────────────────────────────────────────────────
echo "==> Installing to $INSTALL_DIR..."
mkdir -p "$INSTALL_DIR"

unzip -qo "$TMPDIR/$ZIPNAME" -d "$TMPDIR"
BIN_PATH=$(find "$TMPDIR" -name "$BIN_NAME" -type f | head -1)
if [ -z "$BIN_PATH" ]; then
  abort "Extraction failed: binary not found in archive."
fi
cp "$BIN_PATH" "$INSTALL_DIR/$BIN_NAME"
chmod +x "$INSTALL_DIR/$BIN_NAME"

# ── PATH hint ──────────────────────────────────────────────────
case ":$PATH:" in
  *:"$INSTALL_DIR":*) ;;  # already in PATH
  *)
    SHELL_RC=""
    case "$SHELL" in
      */zsh)  SHELL_RC="$HOME/.zshrc" ;;
      */bash) SHELL_RC="$HOME/.bashrc" ;;
      */fish) SHELL_RC="$HOME/.config/fish/config.fish" ;;
    esac
    if [ -n "$SHELL_RC" ]; then
      yellow "==> Adding $INSTALL_DIR to PATH in $SHELL_RC"
      echo "export PATH=\"\$PATH:$INSTALL_DIR\"" >> "$SHELL_RC"
      yellow "    Run 'source $SHELL_RC' or restart your shell to use bialet."
    else
      yellow "==> Add $INSTALL_DIR to your PATH to use bialet."
      yellow "    export PATH=\"\$PATH:$INSTALL_DIR\""
    fi
    ;;
esac

# ── Verify ─────────────────────────────────────────────────────
"$INSTALL_DIR/$BIN_NAME" -v || abort "Installation verification failed."

green "==> 🚲 Bialet ${TAG#v} installed successfully!"
check_runtime_deps
