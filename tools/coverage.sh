#!/bin/bash
#
# Line coverage for the C sources, using the same suites `make check` runs.
#
# Usage: tools/coverage.sh [clang]
#
# Builds an instrumented copy of the binary in build-cov/, runs the integration
# suite plus both in-process suites, and prints per-file line coverage. Vendored
# Wren files are listed separately: they are upstream code, so their percentages
# say less about this project.
#
# Needs clang with --coverage and llvm-cov (or plain gcov for a gcc build).
# Exits 0 with a message when the toolchain cannot provide coverage.
#
# Note: tests/run.sh starts its own server and about a dozen auxiliary ones
# inside individual tests. It normally stops them with SIGKILL, which discards
# their counters and shows 0% for every file the HTTP path touches. This script
# sets BIALET_COVERAGE=1, which makes tests/util.sh stop them gracefully with
# SIGTERM instead (see stop_matching_servers).
#
# Sources with no .gcda at all are listed at 0% with a "(no data)" note, so a
# file that never ran cannot silently disappear from the report.

set -uo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CC="${1:-clang}"
BUILD_DIR="$ROOT_DIR/build-cov"

cd "$ROOT_DIR"

if ! command -v "$CC" >/dev/null 2>&1; then
  echo "coverage: $CC not found, skipping"
  exit 0
fi

GCOV=""
if command -v llvm-cov >/dev/null 2>&1; then
  GCOV="llvm-cov gcov"
elif command -v xcrun >/dev/null 2>&1 && xcrun --find llvm-cov >/dev/null 2>&1; then
  GCOV="xcrun llvm-cov gcov"
elif command -v gcov >/dev/null 2>&1; then
  GCOV="gcov"
else
  echo "coverage: no llvm-cov/gcov found, skipping"
  exit 0
fi

echo "coverage: building instrumented binary in $BUILD_DIR"
make -s BUILD_DIR="$BUILD_DIR" CC="$CC" \
  EXTRA_CFLAGS="--coverage" EXTRA_LDFLAGS="--coverage" || exit 1

rm -f "$BUILD_DIR"/src/*.gcda
find "$ROOT_DIR" -maxdepth 1 -name '*.gcov' -delete

echo "coverage: running tests/run.sh"
BIALET_COVERAGE=1 ./tests/run.sh "$BUILD_DIR/bialet" 127.0.0.1 7691 -q | tail -1
echo "coverage: running -T tests"
"$BUILD_DIR/bialet" -T tests >/dev/null 2>&1
echo "coverage: running -T docs/examples"
"$BUILD_DIR/bialet" -T docs/examples >/dev/null 2>&1

# The .gcda files are written next to the objects, but gcov wants the object
# file so it can find the matching .gcno.
for obj in "$BUILD_DIR"/src/*.c.o; do
  $GCOV -o "$BUILD_DIR/src" "$obj" >/dev/null 2>&1
done

python3 - "$ROOT_DIR" <<'PY'
import glob
import os
import re
import sys

root = sys.argv[1]
srcdir = os.path.join(root, "src")
vendored = lambda f: os.path.basename(f).startswith(("wren_", "getopt"))


def source_lines(path):
    """Approximate executable-line count for a source with no gcov data:
    non-blank, non-comment lines. gcov also counts braces and declarations, so
    this under-counts slightly; the point of the row is that none of it ran."""
    n = 0
    in_block = False
    for line in open(path, errors="replace"):
        s = line.strip()
        if in_block:
            if "*/" in s:
                in_block = False
            continue
        if s.startswith("/*"):
            in_block = "*/" not in s
            continue
        if not s or s.startswith(("//", "*")):
            continue
        n += 1
    return n


# (name, hit, total, measured). measured is False for sources with no coverage
# data at all, which are listed at 0% instead of vanishing from the report.
rows = []
seen = set()
for path in glob.glob(os.path.join(root, "*.gcov")):
    name = os.path.basename(path)[:-5]
    # Header gcov files are inlined code from system headers; only .c sources
    # describe this project (and getopt.c is vendored too).
    if not name.endswith(".c"):
        continue
    total = hit = 0
    for line in open(path, errors="replace"):
        m = re.match(r"\s*([0-9#]+):\s*(\d+):", line)
        if not m:
            continue
        count = m.group(1)
        total += 1
        if count.isdigit() and int(count) > 0:
            hit += 1
    if total:
        rows.append((name, hit, total, True))
        seen.add(name)

# A source the suite never executed leaves no .gcda behind, so gcov writes no
# .gcov for it and the file used to be missing from the report entirely: the
# percentages then described only the code that happened to run. Derive the
# list from src/*.c instead.
for path in sorted(glob.glob(os.path.join(srcdir, "*.c"))):
    name = os.path.basename(path)
    if name in seen:
        continue
    rows.append((name, 0, source_lines(path), False))

own = sorted(r for r in rows if not vendored(r[0]))
up = sorted(r for r in rows if vendored(r[0]))


def show(title, items):
    print("\n%s" % title)
    th = tt = 0
    for name, hit, total, measured in sorted(items, key=lambda r: r[1] / max(r[2], 1)):
        note = "" if measured else "   (no data)"
        print("  %5.1f%%  %-24s %5d/%d%s" % (hit / max(total, 1) * 100, name, hit, total, note))
        th += hit
        tt += total
    if tt:
        print("  %5.1f%%  %-24s %5d/%d" % (th / tt * 100, "TOTAL", th, tt))
    return th, tt


oh, ot = show("Bialet sources", own)
uh, ut = show("Vendored Wren", up)
if ot + ut:
    print("\nOverall: %.1f%% (%d/%d lines)" % ((oh + uh) / (ot + ut) * 100, oh + uh, ot + ut))
print("\n(no data): no .gcda was written, so the file never ran.")
PY
