#!/usr/bin/env bash
#
# stop-sites.sh - stop servers started by start-sites.sh.
#
# Idempotent: a missing manifest is not an error. Stops the RSS sampler first,
# then each supervisor (which forwards SIGTERM to its HTTP child), then any
# child still alive after the timeout.
#
# Usage: stop-sites.sh [options]
#   --manifest FILE   Manifest from start-sites.sh (default: bench/work/run/sites.tsv)
#   --pids-file FILE  Child pid file (default: bench/work/run/sites.pids)
#   --run-dir DIR     Directory holding the sampler pid (default: bench/work/run)
#   --timeout SEC     Grace period per process (default: 10)
#   -h, --help        Show this help

set -euo pipefail

# shellcheck source=lib/common.sh
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/lib/common.sh"

MANIFEST="$RUN_DIR/sites.tsv"
PIDS_FILE="$RUN_DIR/sites.pids"
TIMEOUT=10

usage() {
  print_usage_header "${BASH_SOURCE[0]}"
}

need_value() { (($# >= 2)) || die "$1 needs a value"; }

while (($#)); do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    --manifest) need_value "$@"; MANIFEST="$2"; shift 2 ;;
    --manifest=*) MANIFEST="${1#*=}"; shift ;;
    --pids-file) need_value "$@"; PIDS_FILE="$2"; shift 2 ;;
    --pids-file=*) PIDS_FILE="${1#*=}"; shift ;;
    --run-dir) need_value "$@"; RUN_DIR="$2"; shift 2 ;;
    --run-dir=*) RUN_DIR="${1#*=}"; shift ;;
    --timeout) need_value "$@"; TIMEOUT="$2"; shift 2 ;;
    --timeout=*) TIMEOUT="${1#*=}"; shift ;;
    *) die "unknown option: $1 (try --help)" ;;
  esac
done

is_uint "$TIMEOUT" || die "--timeout must be seconds"

sampler_pid_file="$RUN_DIR/sites-sampler.pid"
if [[ -s "$sampler_pid_file" ]]; then
  sampler_pid="$(<"$sampler_pid_file")"
  if [[ -n "$sampler_pid" ]] && kill -0 "$sampler_pid" 2>/dev/null; then
    if pid_matches "$sampler_pid" "mem.sh"; then
      info "stopping RSS sampler $sampler_pid"
      stop_pid "$sampler_pid" 5
    else
      warn "ignoring stale sampler pid $sampler_pid (not mem.sh)"
    fi
  fi
  rm -f -- "$sampler_pid_file"
fi

if [[ -s "$MANIFEST" ]]; then
  # Read into arrays first: stopping a supervisor while the loop still has the
  # file open is harmless, but a clear list makes the output deterministic.
  supervisors=()
  children=()
  while IFS=$'\t' read -r site port app supervisor child log; do
    [[ "$site" =~ ^# || -z "$site" ]] && continue
    [[ "$supervisor" =~ ^[0-9]+$ ]] && supervisors+=("$supervisor")
    [[ "$child" =~ ^[0-9]+$ ]] && children+=("$child")
  done <"$MANIFEST"

  for pid in "${supervisors[@]:-}"; do
    [[ -n "$pid" ]] || continue
    if kill -0 "$pid" 2>/dev/null; then
      if pid_matches "$pid" "bialet"; then
        info "stopping supervisor $pid"
        stop_pid "$pid" "$TIMEOUT"
      else
        warn "ignoring stale supervisor pid $pid (not bialet)"
      fi
    fi
  done
  for pid in "${children[@]:-}"; do
    [[ -n "$pid" ]] || continue
    if kill -0 "$pid" 2>/dev/null; then
      if pid_matches "$pid" "bialet"; then
        warn "HTTP child $pid survived its supervisor; killing it"
        stop_pid "$pid" 5
      else
        warn "ignoring stale child pid $pid (not bialet)"
      fi
    fi
  done
else
  info "no manifest at $MANIFEST; nothing to stop"
fi

rm -f -- "$MANIFEST" "$PIDS_FILE"
info "stopped"
