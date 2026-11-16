#!/usr/bin/env bash
#
# report.sh - build report.md and pareto.md from a results directory.
#
# Thin wrapper around analyze.py. Defaults to the newest directory under
# bench/results/ that contains a run.env (the harness writes one per run).
#
# Usage: report.sh [options]
#   --results DIR    Results directory (default: newest under bench/results/)
#   --baseline DIR   Earlier results for latency and perf-stat deltas
#   --top N          Rows per table (default: 15)
#   -h, --help       Show this help

set -euo pipefail

# shellcheck source=lib/common.sh
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/lib/common.sh"

RESULTS=""
BASELINE=""
TOP=15

usage() {
  print_usage_header "${BASH_SOURCE[0]}"
}

need_value() { (($# >= 2)) || die "$1 needs a value"; }

while (($#)); do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    --results) need_value "$@"; RESULTS="$2"; shift 2 ;;
    --results=*) RESULTS="${1#*=}"; shift ;;
    --baseline) need_value "$@"; BASELINE="$2"; shift 2 ;;
    --baseline=*) BASELINE="${1#*=}"; shift ;;
    --top) need_value "$@"; TOP="$2"; shift 2 ;;
    --top=*) TOP="${1#*=}"; shift ;;
    *) die "unknown option: $1 (try --help)" ;;
  esac
done

is_uint "$TOP" && ((TOP >= 1)) || die "--top must be a positive integer"
require_cmd python3

if [[ -z "$RESULTS" ]]; then
  latest=""
  for candidate in "$RESULTS_DIR"/*/; do
    [[ -d "$candidate" ]] || continue
    [[ -f "$candidate/run.env" ]] || continue
    latest="${candidate%/}"
  done
  [[ -n "$latest" ]] || die "no results under $RESULTS_DIR; run bench/run.sh baseline"
  RESULTS="$latest"
fi
[[ -d "$RESULTS" ]] || die "results directory not found: $RESULTS"
[[ -f "$RESULTS/run.env" ]] || warn "no run.env in $RESULTS; reporting what is there"

if [[ -n "$BASELINE" ]]; then
  [[ -d "$BASELINE" ]] || die "baseline directory not found: $BASELINE"
  info "baseline: $BASELINE"
fi

args=("$RESULTS" --top "$TOP")
[[ -n "$BASELINE" ]] && args+=(--baseline "$BASELINE")

python3 "$BENCH_DIR/analyze.py" "${args[@]}"

printf '\n'
for name in report.md pareto.md load-summary.json; do
  if [[ -f "$RESULTS/$name" ]]; then
    printf '  %s/%s\n' "$RESULTS" "$name"
  fi
done

# Point at whichever raw sections were collected, so a missing profile is
# obvious without opening the report.
for pair in "load:load runs" "perf:perf stat" "profile:perf record" \
            "alloc:heaptrack" "syscalls:strace" "mem:RSS samples"; do
  dir="${pair%%:*}"
  label="${pair#*:}"
  if [[ -d "$RESULTS/$dir" ]]; then
    count="$(find "$RESULTS/$dir" -maxdepth 2 -type f 2>/dev/null | wc -l | tr -d ' ')"
  else
    count=0
  fi
  printf '  %-16s %s file(s)\n' "$label" "$count"
done
