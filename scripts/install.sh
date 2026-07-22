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

# ── Detect package manager ─────────────────────────────────────
detect_package_manager() {
  if [ "$(uname -s)" = "Darwin" ]; then
    command -v brew >/dev/null 2>&1 && echo "brew" || echo "macos-nobrew"
    return
  fi
  local pm
  for pm in dnf apt-get apk pacman zypper yum; do
    if command -v "$pm" >/dev/null 2>&1; then
      echo "$pm"
      return
    fi
  done
  echo "unknown"
}

pm="$(detect_package_manager)"

sudo_cmd=""
case "$pm" in
  macos-nobrew|unknown) ;;
  brew) ;;
  *) command -v sudo >/dev/null 2>&1 && sudo_cmd="sudo" ;;
esac

# ── Installer dependencies (needed by this script) ──────────────
REQUIRED=(curl tar mktemp)
missing=()
for cmd in "${REQUIRED[@]}"; do
  command -v "$cmd" >/dev/null 2>&1 || missing+=("$cmd")
done

if [ ${#missing[@]} -gt 0 ]; then
  yellow "==> Installing installer dependencies: ${missing[*]}"
  case "$pm" in
    dnf)
      $sudo_cmd dnf install -y "${missing[@]}"
      ;;
    yum)
      $sudo_cmd yum install -y "${missing[@]}"
      ;;
    apt-get)
      $sudo_cmd apt-get update -qq
      DEBIAN_FRONTEND=noninteractive $sudo_cmd apt-get install -y -q "${missing[@]}"
      ;;
    apk)
      $sudo_cmd apk add --no-cache "${missing[@]}"
      ;;
    pacman)
      $sudo_cmd pacman -Sy --noconfirm "${missing[@]}"
      ;;
    zypper)
      $sudo_cmd zypper -n install "${missing[@]}"
      ;;
    brew)
      brew install "${missing[@]}"
      ;;
    macos-nobrew)
      echo "Error: missing ${missing[*]} on macOS without Homebrew." >&2
      echo "Install Homebrew from https://brew.sh and re-run, or install the packages manually." >&2
      exit 1
      ;;
    unknown|*)
      echo "Error: could not detect a supported package manager." >&2
      echo "Detected: OS=$(uname -s), Arch=$(uname -m)" >&2
      echo "Install ${missing[*]} manually and re-run." >&2
      exit 1
      ;;
  esac
fi

# ── App runtime dependencies (macOS only; Linux binary is static) ─
if [ "$OS" = "Darwin" ]; then
  case "$pm" in
    brew)
      brew install sqlite3 curl openssl 2>/dev/null || true
      ;;
    macos-nobrew)
      yellow "==> Warning: cannot install app dependencies without Homebrew."
      yellow "    Install sqlite3, openssl, and curl manually."
      ;;
  esac
fi

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
TARNAME="${BIN_NAME}-${TAG}-${PLATFORM}-${ARCH_NORM}.tar.gz"
DOWNLOAD_URL="https://github.com/$REPO/releases/download/$TAG/$TARNAME"

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

echo "==> Downloading $TARNAME..."
curl -sSLf "$DOWNLOAD_URL" -o "$TMPDIR/$TARNAME" || abort "Download failed. Check that release $TAG exists."

# ── Verify tar ──────────────────────────────────────────────────
if ! tar tzf "$TMPDIR/$TARNAME" > /dev/null 2>&1; then
  abort "Downloaded file is not a valid tar.gz. Release asset may be missing for $TAG."
fi

# ── Install ────────────────────────────────────────────────────
echo "==> Installing to $INSTALL_DIR..."
mkdir -p "$INSTALL_DIR"

tar xzf "$TMPDIR/$TARNAME" -C "$TMPDIR"
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
