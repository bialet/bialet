#!/bin/bash
#
# Windows syntax check without a Windows host.
#
# `tools/crosscompile.sh windows` produces the real binary inside Docker, but it
# downloads SQLite and builds OpenSSL (minutes) and needs a running Docker
# daemon. This script is the fast gate in between: it runs the MinGW compiler
# over every source file with -fsyntax-only, which catches the platform bugs
# that are cheap to introduce and expensive to discover in a release --
# unguarded POSIX calls, `long` assumptions under LLP64, ssize_t/socket types,
# and format specifiers.
#
# It is not a substitute for crosscompiling and running the suite under Wine
# (see AGENTS.md); it only proves the code compiles for _WIN32.
#
# Usage: tools/check_mingw_syntax.sh [compiler]
#
# Exits 0 when it passes, 1 on a compiler error, and 0 with a skip message when
# MinGW (or the SQLite/curl/OpenSSL headers) is not installed.

set -uo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CC="${1:-x86_64-w64-mingw32-gcc}"

if ! command -v "$CC" >/dev/null 2>&1; then
  echo "check_mingw_syntax: $CC not found, skipping (install mingw-w64)"
  exit 0
fi

# Header search paths. On macOS these come from Homebrew; on Linux from the
# mingw sysroot plus the distro's -dev packages. Missing headers make the
# compile fail loudly, which is the point: an empty path silently accepting
# everything would defeat the check.
INCLUDES=()
for pkg in sqlite curl openssl; do
  prefix="$(brew --prefix "$pkg" 2>/dev/null || true)"
  if [[ -n "$prefix" ]]; then
    INCLUDES+=("-I$prefix/include")
  fi
done
for dir in /usr/x86_64-w64-mingw32/include /usr/include; do
  [[ -d "$dir" ]] && INCLUDES+=("-I$dir")
done

# Skip instead of failing when the target headers are absent: this check is an
# optional fast gate, not a release requirement.
for header in sqlite3.h curl/curl.h openssl/ssl.h; do
  if ! echo "#include <$header>" | "$CC" -E "${INCLUDES[@]}" - >/dev/null 2>&1; then
    echo "check_mingw_syntax: Windows $header not found, skipping"
    exit 0
  fi
done

# -Wno-deprecated-declarations: a newer OpenSSL header than the one the Windows
# release links (3.0.x) marks SSL_set1_host deprecated. The deprecation is a
# property of the local header, not of the Windows build.
"$CC" -fsyntax-only -std=gnu17 -Wall -Wextra -Werror \
  -Wno-deprecated-declarations -DHAVE_SSL "${INCLUDES[@]}" \
  "$ROOT_DIR"/src/*.c
status=$?

if [[ $status -ne 0 ]]; then
  echo "check_mingw_syntax: Windows syntax check failed" >&2
  exit 1
fi
echo "check_mingw_syntax: all sources compile for _WIN32"
