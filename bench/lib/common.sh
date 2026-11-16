#!/usr/bin/env bash
# Shared helpers for the Bialet benchmark harness.
#
# Source this file from a script that has already enabled `set -euo pipefail`.
# It is intentionally Linux-first: perf, heaptrack, strace and /proc are the
# probes the harness is built around. Small helpers fall back to portable
# commands so the generators can run on macOS for syntax checks.

if [[ -n "${BENCH_COMMON_SH:-}" ]]; then
  return 0
fi
BENCH_COMMON_SH=1

BENCH_LIB_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BENCH_DIR="$(cd -- "$BENCH_LIB_DIR/.." && pwd)"
REPO_ROOT="$(cd -- "$BENCH_DIR/.." && pwd)"

WORK_DIR="${BENCH_WORK_DIR:-$BENCH_DIR/work}"
RESULTS_DIR="${BENCH_RESULTS_DIR:-$BENCH_DIR/results}"
APPS_DIR="${BENCH_APPS_DIR:-$WORK_DIR/apps}"
RUN_DIR="${BENCH_RUN_DIR:-$WORK_DIR/run}"
LOG_DIR="${BENCH_LOG_DIR:-$WORK_DIR/logs}"

info() { printf '==> %s\n' "$*" >&2; }
warn() { printf 'warning: %s\n' "$*" >&2; }
die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

have() { command -v "$1" >/dev/null 2>&1; }

is_linux() { [[ "$(uname -s)" == Linux ]]; }

is_uint() { [[ "${1:-}" =~ ^[0-9]+$ ]]; }

utc_stamp() { date -u +%Y%m%dT%H%M%SZ; }

ensure_dir() { mkdir -p -- "$1"; }

abs_path() { (cd -- "$1" && pwd); }

require_cmd() {
  local cmd
  for cmd in "$@"; do
    have "$cmd" || die "required command not found: $cmd (run bench/install-tools.sh)"
  done
}

sha256_of() {
  local file="$1"
  if have sha256sum; then
    sha256sum -- "$file" | awk '{print $1}'
  elif have shasum; then
    shasum -a 256 -- "$file" | awk '{print $1}'
  else
    printf 'unavailable'
  fi
}

# Open a TCP connection to host:port once. Uses bash's /dev/tcp so it works on
# a stock Debian box without curl, python or nc.
port_open() {
  local host="$1" port="$2"
  (exec 3<>"/dev/tcp/${host}/${port}") >/dev/null 2>&1
}

# wait_for_port HOST PORT TIMEOUT_SECONDS [SUPERVISOR_PID]
# Fails early if the supervisor exits while waiting.
wait_for_port() {
  local host="$1" port="$2" timeout="$3" pid="${4:-}"
  local deadline=$((SECONDS + timeout))
  while ((SECONDS < deadline)); do
    if port_open "$host" "$port"; then
      return 0
    fi
    if [[ -n "$pid" ]] && ! kill -0 "$pid" 2>/dev/null; then
      return 1
    fi
    sleep 0.1
  done
  return 1
}

# Direct children of PID, space separated. /proc is authoritative on Linux;
# pgrep keeps this usable on macOS for generator smoke tests.
child_pids_of() {
  local pid="$1"
  local file="/proc/$pid/task/$pid/children"
  if [[ -r "$file" ]]; then
    tr '\n' ' ' <"$file"
  elif have pgrep; then
    pgrep -P "$pid" | tr '\n' ' '
  fi
}

# wait_for_child_pid PID [TIMEOUT_SECONDS] -> prints the first child pid.
wait_for_child_pid() {
  local pid="$1" timeout="${2:-10}"
  local deadline=$((SECONDS + timeout)) kids
  while ((SECONDS < deadline)); do
    kids="$(child_pids_of "$pid")"
    if [[ -n "${kids// /}" ]]; then
      # shellcheck disable=SC2086
      set -- $kids
      printf '%s\n' "$1"
      return 0
    fi
    kill -0 "$pid" 2>/dev/null || return 1
    sleep 0.1
  done
  return 1
}

# pid_matches PID PATTERN - true when /proc/PID/cmdline is unavailable or
# contains PATTERN. Guards cleanup against a recycled pid after a reboot.
pid_matches() {
  local pid="$1" pattern="$2" cmd=""
  [[ -r "/proc/$pid/cmdline" ]] || return 0
  cmd="$(tr '\0' ' ' <"/proc/$pid/cmdline" 2>/dev/null || true)"
  [[ -z "$cmd" || "$cmd" == *"$pattern"* ]]
}

# stop_pid PID [TIMEOUT_SECONDS] - SIGTERM, then SIGKILL. Safe if already gone.
stop_pid() {
  local pid="$1" timeout="${2:-10}"
  [[ -n "$pid" ]] || return 0
  kill -0 "$pid" 2>/dev/null || return 0
  kill -TERM "$pid" 2>/dev/null || true
  local deadline=$((SECONDS + timeout))
  while kill -0 "$pid" 2>/dev/null; do
    if ((SECONDS >= deadline)); then
      kill -KILL "$pid" 2>/dev/null || true
      break
    fi
    sleep 0.1
  done
  wait "$pid" 2>/dev/null || true
}

# Run a command, prefixing every line of its combined output. Used for the
# environment metadata files so a failure in one probe does not abort the run.
run_logged() {
  local out="$1" title="$2"
  shift 2
  {
    printf '### %s\n' "$title"
    "$@" 2>&1 || printf '(command failed: %s)\n' "$*"
    printf '\n'
  } >>"$out"
}

json_note() {
  # Keep ad-hoc notes machine-readable without pulling in jq.
  local file="$1" key="$2" value="$3"
  printf '{"key": "%s", "value": "%s"}\n' "$key" "$value" >>"$file"
}

# True when we are inside the repository root. Used only for warnings; scripts
# resolve paths themselves and never depend on the caller's cwd.
in_repo_root() {
  [[ "$(pwd -P)" == "$(cd -- "$REPO_ROOT" && pwd -P)" ]]
}

# Print the comment block at the top of a script, stopping at the first
# non-comment line. Used by every --help handler so usage text cannot drift
# into printing code.
print_usage_header() {
  awk 'NR > 1 { if ($0 ~ /^#/) { sub(/^# ?/, ""); print } else { exit } }' "$1"
}
