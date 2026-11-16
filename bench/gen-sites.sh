#!/usr/bin/env bash
#
# gen-sites.sh - generate N copies of the synthetic benchmark app.
#
# Default is 8 apps under bench/work/apps/site-1 .. site-8, used by the
# multi-site scenario (one server process per app, consecutive ports).
#
# Usage: gen-sites.sh [options]
#   --count N        Number of apps (default: 8)
#   --apps-dir DIR   Parent directory (default: bench/work/apps)
#   --force          Regenerate each app from scratch, deleting its database
#   --validate       Syntax-check every generated .wren file
#   --routes N       Passed to gen-app.sh (default: 120)
#   --seed N         Passed to gen-app.sh (default: 10000)
#   --large-every N  Passed to gen-app.sh (default: 17)
#   --js-bytes N     Passed to gen-app.sh (default: 51200)
#   -h, --help       Show this help

set -euo pipefail

# shellcheck source=lib/common.sh
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/lib/common.sh"

COUNT=8
FORCE=0
VALIDATE=0
ROUTES=120
SEED=10000
LARGE_EVERY=17
JS_BYTES=51200

usage() {
  print_usage_header "${BASH_SOURCE[0]}"
}

need_value() { (($# >= 2)) || die "$1 needs a value"; }

while (($#)); do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    --count) need_value "$@"; COUNT="$2"; shift 2 ;;
    --count=*) COUNT="${1#*=}"; shift ;;
    --apps-dir) need_value "$@"; APPS_DIR="$2"; shift 2 ;;
    --apps-dir=*) APPS_DIR="${1#*=}"; shift ;;
    --force) FORCE=1; shift ;;
    --validate) VALIDATE=1; shift ;;
    --routes) need_value "$@"; ROUTES="$2"; shift 2 ;;
    --routes=*) ROUTES="${1#*=}"; shift ;;
    --seed) need_value "$@"; SEED="$2"; shift 2 ;;
    --seed=*) SEED="${1#*=}"; shift ;;
    --large-every) need_value "$@"; LARGE_EVERY="$2"; shift 2 ;;
    --large-every=*) LARGE_EVERY="${1#*=}"; shift ;;
    --js-bytes) need_value "$@"; JS_BYTES="$2"; shift 2 ;;
    --js-bytes=*) JS_BYTES="${1#*=}"; shift ;;
    *) die "unknown option: $1 (try --help)" ;;
  esac
done

is_uint "$COUNT" && ((COUNT >= 1)) || die "--count must be at least 1"
ensure_dir "$APPS_DIR"

gen_args=(--routes "$ROUTES" --seed "$SEED" --large-every "$LARGE_EVERY"
          --js-bytes "$JS_BYTES")
((FORCE == 1)) && gen_args+=(--force)
((VALIDATE == 1)) && gen_args+=(--validate)

for ((i = 1; i <= COUNT; i++)); do
  out="$APPS_DIR/site-$i"
  "$BENCH_DIR/gen-app.sh" --out "$out" --name "site-$i" "${gen_args[@]}"
done

info "generated $COUNT app(s) under $APPS_DIR"
