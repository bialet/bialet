#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DIST_DIR="$PROJECT_DIR/build"

SQLITE_YEAR="2024"
SQLITE_VERSION="3460100"
OPENSSL_VERSION="3.0.15"
MINGW_PREFIX="x86_64-w64-mingw32"
MINGW_ROOT="/usr/$MINGW_PREFIX"

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

usage() {
    echo "Usage: $0 [linux|windows|all]"
    echo
    echo "  linux    Build static Linux x86_64 binary (Docker ubuntu:22.04)"
    echo "  windows  Cross-compile static Windows x86_64 binary (Docker + MinGW)"
    echo "  all      Build both (default)"
    exit 1
}

TARGET="${1:-all}"
case "$TARGET" in
    linux|windows|all) ;;
    *) usage ;;
esac

step() { echo -e "${CYAN}[*] $1${NC}"; }
ok()   { echo -e "${GREEN}[+] $1${NC}"; }
err()  { echo -e "${RED}[-] $1${NC}"; exit 1; }

mkdir -p "$DIST_DIR"

# ── Linux ──────────────────────────────────────────────────────────
build_linux() {
    step "Building Linux x86_64 (static)"
    docker run --rm -v "$PROJECT_DIR:/src" -w /src ubuntu:22.04 bash -c '
        set -e
        apt-get update -qq && apt-get install -y -qq \
            build-essential make libsqlite3-dev libcurl4-openssl-dev libssl-dev \
            libnghttp2-dev libidn2-dev librtmp-dev libssh-dev libpsl-dev \
            libkrb5-dev libldap2-dev libbrotli-dev libzstd-dev > /dev/null 2>&1
        make clean static
    '
    cp "$PROJECT_DIR/build/bialet" "$DIST_DIR/bialet-linux-x86_64"
    ok "Linux: $DIST_DIR/bialet-linux-x86_64"
}

# ── Windows ─────────────────────────────────────────────────────────
build_windows() {
    step "Cross-compiling Windows x86_64 (static, MinGW)"

    docker run --rm \
        -v "$PROJECT_DIR:/src" \
        -w /src \
        -e SQLITE_YEAR="$SQLITE_YEAR" \
        -e SQLITE_VERSION="$SQLITE_VERSION" \
        -e OPENSSL_VERSION="$OPENSSL_VERSION" \
        ubuntu:22.04 bash -c '
        set -e
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq && apt-get install -y -qq \
            gcc-mingw-w64-x86-64-posix make curl unzip perl > /dev/null 2>&1

        CROSS=x86_64-w64-mingw32
        MINGW_ROOT=/usr/${CROSS}

        echo "  SQLite..."
        mkdir -p /tmp/_bialet_build && cd /tmp/_bialet_build
        curl -fsSL "https://www.sqlite.org/${SQLITE_YEAR}/sqlite-amalgamation-${SQLITE_VERSION}.zip" -o sqlite.zip
        unzip -oq sqlite.zip
        cd sqlite-amalgamation-${SQLITE_VERSION}
        ${CROSS}-gcc -c sqlite3.c -O2 -o sqlite3.o
        ${CROSS}-ar rcs libsqlite3.a sqlite3.o
        install -Dm644 libsqlite3.a ${MINGW_ROOT}/lib/libsqlite3.a
        install -Dm644 sqlite3.h sqlite3ext.h -t ${MINGW_ROOT}/include/

        echo "  OpenSSL (this takes a few minutes)..."
        cd /tmp/_bialet_build
        curl -fsSL "https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz" -o openssl.tar.gz
        tar xf openssl.tar.gz
        cd openssl-${OPENSSL_VERSION}
        ./Configure mingw64 --cross-compile-prefix=${CROSS}- \
            --prefix=${MINGW_ROOT} --libdir=lib no-shared no-tests > /dev/null 2>&1
        make -j$(nproc) > /dev/null 2>&1
        make install_sw > /dev/null 2>&1

        echo "  bialet..."
        cd /src
        make clean > /dev/null 2>&1
        CC=${CROSS}-gcc make static
        rm -rf /tmp/_bialet_build
    '

    if [ -f "$PROJECT_DIR/build/bialet.exe" ]; then
        cp "$PROJECT_DIR/build/bialet.exe" "$DIST_DIR/bialet-windows-x86_64.exe"
    elif [ -f "$PROJECT_DIR/build/bialet" ]; then
        cp "$PROJECT_DIR/build/bialet" "$DIST_DIR/bialet-windows-x86_64.exe"
    else
        err "Windows binary not found in build/"
    fi
    ok "Windows: $DIST_DIR/bialet-windows-x86_64.exe"
}

# ── Main ────────────────────────────────────────────────────────────
echo "Target: $TARGET"
echo

if [ "$TARGET" = "all" ] || [ "$TARGET" = "linux" ]; then
    build_linux
fi

if [ "$TARGET" = "all" ] || [ "$TARGET" = "windows" ]; then
    build_windows
fi

echo
echo -e "${GREEN}Done.${NC}"
ls -lh "$DIST_DIR/" 2>/dev/null
