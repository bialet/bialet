#!/usr/bin/env bash
#
# mem.sh - resident-memory sampling for Bialet servers.
#
# Two modes:
#   Sampler:   mem.sh --pid PID [--pid PID ...] --out FILE [--duration SEC]
#              mem.sh --pids-file FILE --out FILE [--duration 0]
#              Reads /proc/<pid>/status at --hz (default 20) and writes a CSV.
#              --duration 0 samples until SIGTERM. A summary is written next to
#              the CSV as FILE.summary.md.
#
#   Standalone: mem.sh --app DIR --port PORT [--idle 3] [--load-duration 10]
#              Starts build/bialet, records startup RSS, idle RSS, then drives
#              the Python fallback loader at --load-path and records under-load
#              RSS. This mode is local-only by design.
#
# Bialet runs a supervisor plus one HTTP child on Linux. The HTTP child does the
# work, so this script always samples the child (and can sample several sites).
# RSS is VmRSS; VmHWM is the high-water mark since process start and therefore
# never decreases.
#
# Usage: mem.sh --help

set -euo pipefail

# shellcheck source=lib/common.sh
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/lib/common.sh"

usage() {
  cat <<'EOF'
mem.sh - sample RSS / peak RSS of Bialet HTTP child processes.

Sampler mode:
  mem.sh --pid PID [--pid PID ...] --out FILE [options]
  mem.sh --pids-file FILE --out FILE [options]

Standalone mode (starts and stops its own server):
  mem.sh --app DIR --port PORT [options]

Options:
  --pid PID             PID to sample (repeatable)
  --pids-file FILE      Lines of "label pid" (comments with # allowed)
  --manifest FILE       start-sites.sh manifest; samples every child_pid column
  --out FILE            CSV output path (required for sampler mode)
  --hz N                Samples per second, 1-100 (default: 20)
  --duration SEC        Sampler: seconds to run, 0 = until SIGTERM (default: 0)
  --phase NAME          Label rows with this phase (default: sample)
  --phase-file FILE     Read the phase label from FILE before each sample
  --label NAME          Label to use with a single --pid (default: server)

  --app DIR             App directory for standalone mode
  --binary PATH         Server binary (default: <repo>/build/bialet)
  --port PORT           Port for standalone mode (default: 8123)
  --idle SEC            Idle sampling seconds before load (default: 3)
  --load-duration SEC   Seconds of load (default: 10)
  --load-path PATH      Request path for the load (default: /)
  --no-load             Skip the load phase in standalone mode
  -h, --help            Show this help

Examples:
  mem.sh --pid 1234 --out /tmp/rss.csv --duration 10
  mem.sh --pids-file bench/work/run/sites.pids --out bench/results/x/sites.csv --hz 5
  mem.sh --app bench/work/apps/site-1 --port 8123 --load-path /render

Linux only: it reads /proc/<pid>/status.
EOF
}

PIDS=()
PIDS_FILE=""
MANIFEST=""
OUT=""
HZ=20
DURATION=0
PHASE="sample"
PHASE_FILE=""
PHASE_FILE_OWNED=0
LABEL="server"
APP=""
BINARY="$REPO_ROOT/build/bialet"
PORT=8123
IDLE=3
LOAD_DURATION=10
LOAD_PATH="/"
DO_LOAD=1

need_value() {
  (($# >= 2)) || die "$1 needs a value"
}

while (($#)); do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    --pid) need_value "$@"; PIDS+=("$2"); shift 2 ;;
    --pid=*) PIDS+=("${1#*=}"); shift ;;
    --pids-file) need_value "$@"; PIDS_FILE="$2"; shift 2 ;;
    --pids-file=*) PIDS_FILE="${1#*=}"; shift ;;
    --manifest) need_value "$@"; MANIFEST="$2"; shift 2 ;;
    --manifest=*) MANIFEST="${1#*=}"; shift ;;
    --out) need_value "$@"; OUT="$2"; shift 2 ;;
    --out=*) OUT="${1#*=}"; shift ;;
    --hz) need_value "$@"; HZ="$2"; shift 2 ;;
    --hz=*) HZ="${1#*=}"; shift ;;
    --duration) need_value "$@"; DURATION="$2"; shift 2 ;;
    --duration=*) DURATION="${1#*=}"; shift ;;
    --phase) need_value "$@"; PHASE="$2"; shift 2 ;;
    --phase=*) PHASE="${1#*=}"; shift ;;
    --phase-file) need_value "$@"; PHASE_FILE="$2"; shift 2 ;;
    --phase-file=*) PHASE_FILE="${1#*=}"; shift ;;
    --label) need_value "$@"; LABEL="$2"; shift 2 ;;
    --label=*) LABEL="${1#*=}"; shift ;;
    --app) need_value "$@"; APP="$2"; shift 2 ;;
    --app=*) APP="${1#*=}"; shift ;;
    --binary) need_value "$@"; BINARY="$2"; shift 2 ;;
    --binary=*) BINARY="${1#*=}"; shift ;;
    --port) need_value "$@"; PORT="$2"; shift 2 ;;
    --port=*) PORT="${1#*=}"; shift ;;
    --idle) need_value "$@"; IDLE="$2"; shift 2 ;;
    --idle=*) IDLE="${1#*=}"; shift ;;
    --load-duration) need_value "$@"; LOAD_DURATION="$2"; shift 2 ;;
    --load-duration=*) LOAD_DURATION="${1#*=}"; shift ;;
    --load-path) need_value "$@"; LOAD_PATH="$2"; shift 2 ;;
    --load-path=*) LOAD_PATH="${1#*=}"; shift ;;
    --no-load) DO_LOAD=0; shift ;;
    *) die "unknown option: $1 (try --help)" ;;
  esac
done

is_linux || die "mem.sh reads /proc/<pid>/status and only runs on Linux"
is_uint "$HZ" && ((HZ >= 1 && HZ <= 100)) || die "--hz must be 1-100"
is_uint "$DURATION" || die "--duration must be 0 or more seconds"
is_uint "$IDLE" || die "--idle must be seconds"
is_uint "$LOAD_DURATION" || die "--load-duration must be whole seconds"
is_uint "$PORT" && ((PORT > 0 && PORT <= 65535)) || die "--port must be 1-65535"

INTERVAL="$(awk -v hz="$HZ" 'BEGIN { printf "%.4f", 1.0 / hz }')"
START_EPOCH="$(date +%s)"
MEM_STOP=0
SERVER_PID=""
SAMPLER_PID=""

now_epoch() {
  if [[ -n "${EPOCHREALTIME:-}" ]]; then
    printf '%s' "${EPOCHREALTIME/,/.}"
  else
    date +%s
  fi
}

cleanup() {
  local rc=$?
  trap - EXIT INT TERM
  if [[ -n "$SAMPLER_PID" ]]; then
    kill -TERM "$SAMPLER_PID" 2>/dev/null || true
    wait "$SAMPLER_PID" 2>/dev/null || true
  fi
  if [[ -n "$SERVER_PID" ]]; then
    stop_pid "$SERVER_PID" 10
  fi
  [[ -n "$PHASE_FILE" && "$PHASE_FILE_OWNED" == "1" ]] && rm -f -- "$PHASE_FILE"
  exit "$rc"
}

# Read VmRSS / VmHWM / VmSize / Threads from /proc without spawning awk per
# sample: this loop runs at 20 Hz and must not perturb the server.
read_proc_status() {
  local pid="$1"
  local rss="" hwm="" size="" threads=""
  local key value
  [[ -r "/proc/$pid/status" ]] || return 1
  while IFS=':' read -r key value; do
    case "$key" in
      VmRSS) rss="${value//[!0-9]/}" ;;
      VmHWM) hwm="${value//[!0-9]/}" ;;
      VmSize) size="${value//[!0-9]/}" ;;
      Threads) threads="${value//[!0-9]/}" ;;
    esac
  done <"/proc/$pid/status"
  printf '%s,%s,%s,%s\n' "${rss:-0}" "${hwm:-0}" "${size:-0}" "${threads:-0}"
}

# Rebuild PIDS from the fixed --pid list plus the reloadable files. The files
# are re-read every second so a respawned HTTP child is picked up without
# restarting the sampler.
BASE_PIDS=()
BASE_LABELS=()
refresh_pids() {
  local line label pid
  PIDS=("${BASE_PIDS[@]}")
  PID_LABELS=("${BASE_LABELS[@]}")
  if [[ -n "$PIDS_FILE" && -r "$PIDS_FILE" ]]; then
    while IFS= read -r line || [[ -n "$line" ]]; do
      line="${line%$'\r'}"
      [[ "$line" =~ ^[[:space:]]*$ || "$line" =~ ^[[:space:]]*# ]] && continue
      # Accept "label pid" or a bare pid.
      if [[ "$line" =~ ^([^[:space:]]+)[[:space:]]+([0-9]+) ]]; then
        label="${BASH_REMATCH[1]}"
        pid="${BASH_REMATCH[2]}"
      elif [[ "$line" =~ ^([0-9]+) ]]; then
        label="pid-${BASH_REMATCH[1]}"
        pid="${BASH_REMATCH[1]}"
      else
        continue
      fi
      PID_LABELS+=("$label")
      PIDS+=("$pid")
    done <"$PIDS_FILE"
  fi
  if [[ -n "$MANIFEST" && -r "$MANIFEST" ]]; then
    while IFS=$'\t' read -r site port app supervisor child log; do
      [[ "$site" =~ ^# || -z "$site" ]] && continue
      is_uint "$child" || continue
      PID_LABELS+=("$site")
      PIDS+=("$child")
    done <"$MANIFEST"
  fi
}

sample_loop() {
  local header_written=0
  local last_refresh=$SECONDS
  local phase pid label stats elapsed
  while [[ "$MEM_STOP" == "0" ]]; do
    if ((SECONDS - last_refresh >= 1)) || ((${#PIDS[@]} == 0)); then
      refresh_pids
      last_refresh=$SECONDS
    fi
    if [[ -n "$PHASE_FILE" && -r "$PHASE_FILE" ]]; then
      phase="$(<"$PHASE_FILE")"
    else
      phase="$PHASE"
    fi
    elapsed="$(awk -v now="$(now_epoch)" -v start="$START_EPOCH" \
      'BEGIN { printf "%.2f", now - start }')"
    if [[ "$header_written" == "0" ]]; then
      printf 'phase,elapsed_s,pid,label,vmrss_kb,vmhwm_kb,vmsize_kb,threads\n' >>"$OUT"
      header_written=1
    fi
    if ((${#PIDS[@]} > 0)); then
      local i
      for i in "${!PIDS[@]}"; do
        pid="${PIDS[$i]}"
        label="${PID_LABELS[$i]:-$LABEL}"
        if stats="$(read_proc_status "$pid")"; then
          printf '%s,%s,%s,%s,%s\n' "$phase" "$elapsed" "$pid" "$label" "$stats" >>"$OUT"
        fi
      done
    fi
    if ((DURATION > 0)) && ((SECONDS - START_EPOCH >= DURATION)); then
      break
    fi
    sleep "$INTERVAL" || true
  done
}

summarize() {
  local summary="$OUT.summary.md"
  {
    printf '# Memory sample\n\n'
    printf 'CSV: `%s`\n\n' "$OUT"
    printf 'Sampled at %s Hz with /proc/<pid>/status. VmRSS is resident set size; ' "$HZ"
    printf 'VmHWM is the peak since the process started.\n\n'
    if [[ ! -s "$OUT" ]]; then
      printf 'No samples were collected.\n'
      return 0
    fi
    printf '| phase | pid | label | samples | rss min kB | rss median kB | rss max kB | peak HWM kB |\n'
    printf '| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |\n'
    awk -F, 'NR > 1 && $5 != "" {
        key = $1 SUBSEP $3 SUBSEP $4
        rss = $5 + 0; hwm = $6 + 0
        n[key]++
        if (n[key] == 1 || rss < min[key]) min[key] = rss
        if (n[key] == 1 || rss > max[key]) max[key] = rss
        if (hwm > peak[key]) peak[key] = hwm
        vals[key, n[key]] = rss
      }
      END {
        for (key in n) {
          split(key, parts, SUBSEP)
          m = n[key]; asort_done = 0
          # Insertion sort the (small) sample set for the median.
          for (i = 1; i <= m; i++) a[i] = vals[key, i]
          for (i = 2; i <= m; i++) {
            v = a[i]; j = i - 1
            while (j > 0 && a[j] > v) { a[j+1] = a[j]; j-- }
            a[j+1] = v
          }
          med = (m % 2) ? a[(m+1)/2] : (a[m/2] + a[m/2+1]) / 2
          printf "| %s | %s | %s | %d | %d | %.0f | %d | %d |\n",
                 parts[1], parts[2], parts[3], m, min[key], med, max[key], peak[key]
        }
      }' "$OUT"
    printf '\nPeak RSS per phase (sum of each process peak, an upper bound for multi-site):\n\n'
    awk -F, 'NR > 1 && $5 != "" {
        key = $1 SUBSEP $3
        if ($5 + 0 > peak[key]) peak[key] = $5 + 0
      }
      END {
        for (key in peak) {
          split(key, parts, SUBSEP)
          total[parts[1]] += peak[key]
        }
        for (phase in total) printf "- %s: %d kB\n", phase, total[phase]
      }' "$OUT"
  } >"$summary"
  printf 'memory summary: %s\n' "$summary"
}

# ---------------------------------------------------------------------------
# Standalone mode
# ---------------------------------------------------------------------------

if [[ -n "$APP" ]]; then
  APP="$(abs_path "$APP")"
  [[ -x "$BINARY" ]] || die "server binary not found or not executable: $BINARY"
  BINARY="$(abs_path "$(dirname -- "$BINARY")")/$(basename -- "$BINARY")"
  trap cleanup EXIT INT TERM

  PHASE_FILE="$(mktemp "${TMPDIR:-/tmp}/bialet-mem-phase.XXXXXX")"
  PHASE_FILE_OWNED=1
  printf 'startup\n' >"$PHASE_FILE"
  if [[ -z "$OUT" ]]; then
    stamp="$(utc_stamp)"
    OUT="$RESULTS_DIR/$stamp/mem/standalone.csv"
  fi
  ensure_dir "$(dirname -- "$OUT")"
  : >"$OUT"

  info "starting $BINARY on 127.0.0.1:$PORT"
  ( cd -- "$APP" && exec "$BINARY" -q -p "$PORT" "$APP" ) \
    >"$(dirname -- "$OUT")/server.log" 2>&1 &
  SERVER_PID=$!
  wait_for_port 127.0.0.1 "$PORT" 30 "$SERVER_PID" \
    || die "server did not listen on port $PORT (see $(dirname -- "$OUT")/server.log)"
  child_pid_var="$(wait_for_child_pid "$SERVER_PID" 10 || true)"
  [[ -n "$child_pid_var" ]] || die "could not find the Bialet HTTP child process"
  PIDS=("$child_pid_var")
  PID_LABELS=("server")
  BASE_PIDS=("$child_pid_var")
  BASE_LABELS=("server")
  LABEL="server"
  DURATION=0
  SAMPLER_PID=""
  sample_loop &
  SAMPLER_PID=$!

  sleep 0.5
  info "idle sampling for ${IDLE}s"
  printf 'idle\n' >"$PHASE_FILE"
  sleep "$IDLE"

  if [[ "$DO_LOAD" == "1" ]]; then
    info "load sampling for ${LOAD_DURATION}s: http://127.0.0.1:$PORT$LOAD_PATH"
    printf 'load\n' >"$PHASE_FILE"
    python3 "$BENCH_DIR/lib/load.py" --duration "$LOAD_DURATION" \
      --connections "${BENCH_CONNECTIONS:-4}" \
      "http://127.0.0.1:$PORT$LOAD_PATH" \
      >"$(dirname -- "$OUT")/load.json" 2>"$(dirname -- "$OUT")/load.err" || \
      warn "load generator failed; see $(dirname -- "$OUT")/load.err"
  fi

  printf 'done\n' >"$PHASE_FILE"
  MEM_STOP=1
  kill -TERM "$SAMPLER_PID" 2>/dev/null || true
  wait "$SAMPLER_PID" 2>/dev/null || true
  SAMPLER_PID=""
  summarize
  if [[ -s "$(dirname -- "$OUT")/load.json" ]]; then
    printf '\nLoad result: `%s`\n' "$(dirname -- "$OUT")/load.json"
  fi
  exit 0
fi

# ---------------------------------------------------------------------------
# Sampler mode
# ---------------------------------------------------------------------------

[[ -n "$OUT" ]] || die "sampler mode needs --out FILE"
[[ -n "$PIDS_FILE" || -n "$MANIFEST" || ${#PIDS[@]} -gt 0 ]] \
  || die "sampler mode needs --pid, --pids-file or --manifest"
ensure_dir "$(dirname -- "$OUT")"
: >"$OUT"

# Normalise --pid into parallel arrays, then append file/manifest entries.
if ((${#PIDS[@]} > 0)); then
  _pids=("${PIDS[@]}")
  PIDS=()
  PID_LABELS=()
  for _pid in "${_pids[@]}"; do
    is_uint "$_pid" || die "invalid pid: $_pid"
    PIDS+=("$_pid")
    PID_LABELS+=("$LABEL")
  done
else
  PID_LABELS=()
fi
BASE_PIDS=("${PIDS[@]}")
BASE_LABELS=("${PID_LABELS[@]}")
refresh_pids

trap 'MEM_STOP=1' INT TERM
trap 'MEM_STOP=1' EXIT
sample_loop
trap - INT TERM EXIT
summarize
