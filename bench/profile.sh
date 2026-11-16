#!/usr/bin/env bash
#
# profile.sh - perf record around a load run, then flat + cumulative reports
# and a flamegraph SVG when the FlameGraph scripts are installed.
#
# Attaches to the Bialet HTTP child (not the supervisor): on Linux the child
# owns request handling. Degrades gracefully when perf is not permitted or the
# FlameGraph scripts are missing.
#
# Usage: profile.sh --pid PID --url URL [options]
#   --pid PID            Bialet HTTP child pid (from start-sites.sh manifest)
#   --url URL            Load target; repeat for a mixed run
#   --out DIR            Output directory (default: results/<UTC>/profile)
#   --label NAME         File prefix (default: profile)
#   --duration SEC       Load + record seconds (default: 10)
#   --connections N      Load concurrency (default: 4)
#   --method METHOD      HTTP method (default: GET)
#   --data BODY          Request body
#   --content-type TYPE  Body content type
#   --hz N               perf sampling frequency (default: 999)
#   --call-graph MODE    dwarf or fp (default: dwarf)
#   --no-flamegraph      Skip SVG generation
#   -h, --help           Show this help
#
# Set BENCH_HOST to run the load client over ssh (see README).

set -euo pipefail

# shellcheck source=lib/common.sh
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/lib/common.sh"
# shellcheck source=lib/load.sh
source "$BENCH_DIR/lib/load.sh"

PID=""
OUT=""
LABEL="profile"
DURATION=10
CONNECTIONS="${BENCH_CONNECTIONS:-4}"
METHOD="GET"
BODY=""
CONTENT_TYPE=""
FREQ=999
CALL_GRAPH="dwarf"
DO_FLAME=1
URLS=()

usage() {
  print_usage_header "${BASH_SOURCE[0]}"
}

need_value() { (($# >= 2)) || die "$1 needs a value"; }

while (($#)); do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    --pid) need_value "$@"; PID="$2"; shift 2 ;;
    --pid=*) PID="${1#*=}"; shift ;;
    --url) need_value "$@"; URLS+=("$2"); shift 2 ;;
    --url=*) URLS+=("${1#*=}"); shift ;;
    --out) need_value "$@"; OUT="$2"; shift 2 ;;
    --out=*) OUT="${1#*=}"; shift ;;
    --label) need_value "$@"; LABEL="$2"; shift 2 ;;
    --label=*) LABEL="${1#*=}"; shift ;;
    --duration) need_value "$@"; DURATION="$2"; shift 2 ;;
    --duration=*) DURATION="${1#*=}"; shift ;;
    --connections) need_value "$@"; CONNECTIONS="$2"; shift 2 ;;
    --connections=*) CONNECTIONS="${1#*=}"; shift ;;
    --method) need_value "$@"; METHOD="$2"; shift 2 ;;
    --method=*) METHOD="${1#*=}"; shift ;;
    --data) need_value "$@"; BODY="$2"; shift 2 ;;
    --data=*) BODY="${1#*=}"; shift ;;
    --content-type) need_value "$@"; CONTENT_TYPE="$2"; shift 2 ;;
    --content-type=*) CONTENT_TYPE="${1#*=}"; shift ;;
    --hz) need_value "$@"; FREQ="$2"; shift 2 ;;
    --hz=*) FREQ="${1#*=}"; shift ;;
    --call-graph) need_value "$@"; CALL_GRAPH="$2"; shift 2 ;;
    --call-graph=*) CALL_GRAPH="${1#*=}"; shift ;;
    --no-flamegraph) DO_FLAME=0; shift ;;
    *) die "unknown option: $1 (try --help)" ;;
  esac
done

[[ -n "$PID" ]] || die "--pid is required"
is_uint "$PID" || die "--pid must be a number"
kill -0 "$PID" 2>/dev/null || die "pid $PID is not running"
is_uint "$DURATION" && ((DURATION >= 1)) || die "--duration must be at least 1 second"
is_uint "$CONNECTIONS" && ((CONNECTIONS >= 1)) || die "--connections must be at least 1"
is_uint "$FREQ" && ((FREQ >= 1)) || die "--hz must be a positive integer"
((${#URLS[@]} > 0)) || die "at least one --url is required"
case "$CALL_GRAPH" in dwarf|fp|lbr) ;; *) die "--call-graph must be dwarf, fp or lbr" ;; esac
require_cmd perf
is_linux || die "perf profiling is Linux-only"

if [[ -z "$OUT" ]]; then
  OUT="$RESULTS_DIR/$(utc_stamp)/profile"
fi
ensure_dir "$OUT"

WL_NAME="$LABEL"
WL_METHOD="$METHOD"
WL_BODY="$BODY"
WL_CONTENT_TYPE="$CONTENT_TYPE"

load_init

DATA="$OUT/$LABEL.data"
PERF_LOG="$OUT/$LABEL.perf.log"
REC_PID=""
cleanup() {
  local rc=$?
  trap - EXIT INT TERM
  if [[ -n "$REC_PID" ]] && kill -0 "$REC_PID" 2>/dev/null; then
    kill -INT "$REC_PID" 2>/dev/null || true
    wait "$REC_PID" 2>/dev/null || true
  fi
  exit "$rc"
}
trap cleanup EXIT INT TERM

info "perf record: pid $PID, ${DURATION}s, -F $FREQ, --call-graph $CALL_GRAPH"
# sleep ends the recording; perf flushes its data on exit. Running it in the
# background lets the load start immediately after the syscall is attached.
perf record -o "$DATA" -F "$FREQ" -g --call-graph "$CALL_GRAPH" \
  -p "$PID" -- sleep "$DURATION" >"$PERF_LOG" 2>&1 &
REC_PID=$!
sleep 0.5

load_run "$OUT/$LABEL" "$DURATION" "$CONNECTIONS" "${URLS[@]}" \
  >"$OUT/$LABEL.format" 2>"$OUT/$LABEL.load.err" || \
  warn "load run failed; see $OUT/$LABEL.load.err"

wait "$REC_PID" || warn "perf record exited non-zero; see $PERF_LOG"
REC_PID=""

if [[ ! -s "$DATA" ]]; then
  warn "perf produced no data. If records were dropped for permissions, run:"
  warn "  sudo sysctl -w kernel.perf_event_paranoid=1"
fi

report_one() {
  local mode="$1" outfile="$2"
  shift 2
  if ! perf report --stdio -i "$DATA" --percent-limit 0.05 "$@" \
      >"$outfile" 2>"$outfile.err"; then
    warn "perf report ($mode) failed; see $outfile.err"
    return 0
  fi
  printf 'wrote %s\n' "$outfile"
}

PERCENT_ARGS=()
if perf report --help 2>&1 | grep -q -- "--percent-limit"; then
  PERCENT_ARGS=(--percent-limit 0.05)
fi

if [[ -s "$DATA" ]]; then
  # Self time: --no-children so a wrapper does not absorb its callees.
  report_one flat "$OUT/$LABEL.flat.txt" --no-children --sort symbol \
    "${PERCENT_ARGS[@]}"
  # Inclusive time: --children sums the callchain below each symbol.
  report_one cumulative "$OUT/$LABEL.cumulative.txt" --children --sort symbol \
    "${PERCENT_ARGS[@]}"
  # Source file attribution, when this perf supports it. analyze.py groups the
  # Pareto table by the path that appears here.
  if perf report --help 2>&1 | grep -q -- "--srcline"; then
    report_one srcline "$OUT/$LABEL.srcline.txt" --no-children --sort symbol \
      --srcline "${PERCENT_ARGS[@]}"
  else
    warn "this perf has no --srcline; the Pareto table will say (no debug info)"
  fi
fi

if ((DO_FLAME == 1)) && [[ -s "$DATA" ]]; then
  COLLAPSE="$(command -v stackcollapse-perf.pl 2>/dev/null || true)"
  FLAME="$(command -v flamegraph.pl 2>/dev/null || true)"
  if [[ -z "$COLLAPSE" || -z "$FLAME" ]]; then
    for dir in "${FLAMEGRAPH_DIR:-}" /opt/FlameGraph "$HOME/FlameGraph"; do
      [[ -n "$dir" && -x "$dir/stackcollapse-perf.pl" ]] && COLLAPSE="$dir/stackcollapse-perf.pl"
      [[ -n "$dir" && -x "$dir/flamegraph.pl" ]] && FLAME="$dir/flamegraph.pl"
      [[ -n "$COLLAPSE" && -n "$FLAME" ]] && break
    done
  fi
  if [[ -n "$COLLAPSE" && -n "$FLAME" ]]; then
    if perf script -i "$DATA" 2>"$OUT/$LABEL.script.err" |
        "$COLLAPSE" 2>>"$OUT/$LABEL.script.err" |
        "$FLAME" --title "bialet $LABEL" >"$OUT/$LABEL.svg" 2>>"$OUT/$LABEL.script.err"; then
      printf 'wrote %s\n' "$OUT/$LABEL.svg"
    else
      warn "flamegraph generation failed; see $OUT/$LABEL.script.err"
      rm -f -- "$OUT/$LABEL.svg"
    fi
  else
    cat >&2 <<'EOF'
flamegraph skipped: stackcollapse-perf.pl / flamegraph.pl not found.
Install them with bench/install-tools.sh, or:
  git clone --depth 1 https://github.com/brendangregg/FlameGraph /opt/FlameGraph
  export PATH="/opt/FlameGraph:$PATH"
EOF
  fi
fi

printf '\nperf output: %s\n' "$OUT"
if [[ -s "$OUT/$LABEL.flat.txt" ]]; then
  printf '\nTop self-time symbols:\n'
  grep -E '^[[:space:]]*[0-9]+\.[0-9]+%' "$OUT/$LABEL.flat.txt" 2>/dev/null |
    head -n 15 || true
fi
