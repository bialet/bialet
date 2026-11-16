#!/usr/bin/env bash
#
# run.sh - orchestrator for the Bialet performance harness.
#
# Modes:
#   baseline   Build, generate apps, start servers, warm up, run every workload
#              N times, record latency/throughput and RSS. No profilers.
#   measure    baseline + one `perf stat` window per workload (cycles,
#              instructions, branch/cache misses).
#   profile    perf record -g --call-graph dwarf around the primary workload.
#   alloc      heaptrack attach around the primary workload.
#   syscalls   strace -c attach around the primary workload.
#   all        baseline + measure + profile + alloc + syscalls.
#
# A subcommand is mandatory: the script never starts a load run just because it
# was invoked. Set DRY_RUN=1 to print the plan without building or loading.
#
# Usage: bench/run.sh MODE [options]
#   --sites N              Servers/apps to run (default: 1)
#   --routes N             Generated routes per app (default: 120)
#   --seed N               Seeded rows per app (default: 10000)
#   --port-base N          First port (default: 8100)
#   --cache on|off         Pass -x to the server (default: on)
#   --wal on|off           Pass -w to the server (default: on)
#   --flags STRING         Raw server flags; overrides --cache/--wal
#   --duration SEC         Seconds per load run (default: 10)
#   --connections N        Load concurrency (default: 4)
#   --repeats N            Baseline/measure repeats (default: 5)
#   --warmup SEC           Warm-up load per workload (default: 2; 0 = skip)
#   --cooldown SEC         Pause between repeats (default: 2)
#   --workload NAME        Restrict measurement workloads (repeatable)
#   --profile-workload NAME  Workload for profile/alloc/syscalls (default: mixed)
#   --primary NAME         Alias for --profile-workload
#   --binary PATH          Server binary (default: build/bialet)
#   --results DIR          Results directory (default: bench/results/<UTC>)
#   --no-build             Do not run make
#   --clean-build          make clean before make (also deletes app DBs)
#   --reset-db             Regenerate apps with --force (fresh 10k-row seed)
#   --no-report            Skip report.sh at the end
#   --compare DIR          Baseline results dir for report.sh
#   -h, --help             Show this help
#
# Environment:
#   EXTRA_CFLAGS, EXTRA_LDFLAGS   passed to make (LTO/PGO A/B)
#   BENCH_HOST                    ssh target for the load client
#   BENCH_TARGET_HOST             server address as seen by that client
#   BENCH_BIND_HOST               server listen address (auto)
#   BENCH_LOAD_IMPL               auto|oha|wrk|python
#   BENCH_DURATION, BENCH_CONNECTIONS, BENCH_REPEATS, BENCH_WARMUP,
#   BENCH_COOLDOWN, BENCH_SITES, BENCH_FLAGS, BENCH_CACHE, BENCH_WAL,
#   BENCH_RSS_HZ, DRY_RUN
#
# macOS cannot run this harness: perf, heaptrack, strace and /proc are Linux.

set -euo pipefail

# shellcheck source=lib/common.sh
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/lib/common.sh"
# shellcheck source=lib/load.sh
source "$BENCH_DIR/lib/load.sh"

ALL_WORKLOADS=(static-small static-large html-cpu db-read db-write json session mixed)

MODE=""
SITES="${BENCH_SITES:-1}"
ROUTES="${BENCH_ROUTES:-120}"
SEED="${BENCH_SEED:-10000}"
PORT_BASE="${BENCH_PORT_BASE:-8100}"
CACHE="${BENCH_CACHE:-on}"
WAL="${BENCH_WAL:-on}"
FLAGS_OVERRIDE="${BENCH_FLAGS:-}"
DURATION="${BENCH_DURATION:-10}"
CONNECTIONS="${BENCH_CONNECTIONS:-4}"
REPEATS="${BENCH_REPEATS:-5}"
WARMUP="${BENCH_WARMUP:-2}"
COOLDOWN="${BENCH_COOLDOWN:-2}"
PRIMARY="mixed"
BINARY="$REPO_ROOT/build/bialet"
RESULTS=""
NO_BUILD=0
CLEAN_BUILD=0
RESET_DB=0
NO_REPORT=0
COMPARE=""
SELECTED=()
PROFILE_SELECTED=()
PROFILE_SET=0
RSS_HZ="${BENCH_RSS_HZ:-}"

usage() {
  print_usage_header "${BASH_SOURCE[0]}"
}

need_value() { (($# >= 2)) || die "$1 needs a value"; }

parse_onoff() {
  case "$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')" in
    on|1|yes|true) printf 'on' ;;
    off|0|no|false) printf 'off' ;;
    *) die "expected on/off, got: $1" ;;
  esac
}

while (($#)); do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    baseline|measure|profile|alloc|syscalls|all)
      [[ -z "$MODE" ]] || die "only one mode may be given (already: $MODE)"
      MODE="$1"; shift ;;
    --sites) need_value "$@"; SITES="$2"; shift 2 ;;
    --sites=*) SITES="${1#*=}"; shift ;;
    --routes) need_value "$@"; ROUTES="$2"; shift 2 ;;
    --routes=*) ROUTES="${1#*=}"; shift ;;
    --seed) need_value "$@"; SEED="$2"; shift 2 ;;
    --seed=*) SEED="${1#*=}"; shift ;;
    --port-base) need_value "$@"; PORT_BASE="$2"; shift 2 ;;
    --port-base=*) PORT_BASE="${1#*=}"; shift ;;
    --cache) need_value "$@"; CACHE="$(parse_onoff "$2")"; shift 2 ;;
    --cache=*) CACHE="$(parse_onoff "${1#*=}")"; shift ;;
    --wal) need_value "$@"; WAL="$(parse_onoff "$2")"; shift 2 ;;
    --wal=*) WAL="$(parse_onoff "${1#*=}")"; shift ;;
    --flags) need_value "$@"; FLAGS_OVERRIDE="$2"; shift 2 ;;
    --flags=*) FLAGS_OVERRIDE="${1#*=}"; shift ;;
    --duration) need_value "$@"; DURATION="$2"; shift 2 ;;
    --duration=*) DURATION="${1#*=}"; shift ;;
    --connections) need_value "$@"; CONNECTIONS="$2"; shift 2 ;;
    --connections=*) CONNECTIONS="${1#*=}"; shift ;;
    --repeats) need_value "$@"; REPEATS="$2"; shift 2 ;;
    --repeats=*) REPEATS="${1#*=}"; shift ;;
    --warmup) need_value "$@"; WARMUP="$2"; shift 2 ;;
    --warmup=*) WARMUP="${1#*=}"; shift ;;
    --cooldown) need_value "$@"; COOLDOWN="$2"; shift 2 ;;
    --cooldown=*) COOLDOWN="${1#*=}"; shift ;;
    --workload) need_value "$@"; SELECTED+=("$2"); shift 2 ;;
    --workload=*) SELECTED+=("${1#*=}"); shift ;;
    --profile-workload) need_value "$@"; PROFILE_SELECTED+=("$2"); PROFILE_SET=1; shift 2 ;;
    --profile-workload=*) PROFILE_SELECTED+=("${1#*=}"); PROFILE_SET=1; shift ;;
    --primary) need_value "$@"; PRIMARY="$2"; shift 2 ;;
    --primary=*) PRIMARY="${1#*=}"; shift ;;
    --binary) need_value "$@"; BINARY="$2"; shift 2 ;;
    --binary=*) BINARY="${1#*=}"; shift ;;
    --results) need_value "$@"; RESULTS="$2"; shift 2 ;;
    --results=*) RESULTS="${1#*=}"; shift ;;
    --no-build) NO_BUILD=1; shift ;;
    --clean-build) CLEAN_BUILD=1; shift ;;
    --reset-db) RESET_DB=1; shift ;;
    --no-report) NO_REPORT=1; shift ;;
    --compare) need_value "$@"; COMPARE="$2"; shift 2 ;;
    --compare=*) COMPARE="${1#*=}"; shift ;;
    -*) die "unknown option: $1 (try --help)" ;;
    *) die "unexpected argument: $1 (modes: baseline, measure, profile, alloc, syscalls, all)" ;;
  esac
done

[[ -n "$MODE" ]] || { usage >&2; exit 2; }

is_uint "$SITES" && ((SITES >= 1)) || die "--sites must be at least 1"
is_uint "$ROUTES" || die "--routes must be a number"
is_uint "$SEED" || die "--seed must be a number"
is_uint "$PORT_BASE" && ((PORT_BASE >= 1 && PORT_BASE + SITES - 1 <= 65535)) \
  || die "--port-base/--sites exceed the port range"
is_uint "$DURATION" && ((DURATION >= 1)) || die "--duration must be at least 1 second"
is_uint "$CONNECTIONS" && ((CONNECTIONS >= 1)) || die "--connections must be at least 1"
is_uint "$REPEATS" && ((REPEATS >= 1)) || die "--repeats must be at least 1"
is_uint "$WARMUP" || die "--warmup must be seconds (0 disables)"
is_uint "$COOLDOWN" || die "--cooldown must be seconds"
[[ -f "$BENCH_DIR/workloads/$PRIMARY.txt" ]] || die "unknown --primary workload: $PRIMARY (see bench/workloads/)"

if [[ -z "$RSS_HZ" ]]; then
  if ((SITES > 4)); then RSS_HZ=5; else RSS_HZ=20; fi
fi
is_uint "$RSS_HZ" && ((RSS_HZ >= 1 && RSS_HZ <= 100)) || die "BENCH_RSS_HZ must be 1-100"

# Resolve the workload selection. Profiling modes default to one workload.
if ((${#SELECTED[@]} > 0)); then
  for wl in "${SELECTED[@]}"; do
    [[ -f "$BENCH_DIR/workloads/$wl.txt" ]] || die "unknown workload: $wl"
  done
else
  case "$MODE" in
    baseline|measure|all) SELECTED=("${ALL_WORKLOADS[@]}") ;;
    *) SELECTED=("$PRIMARY") ;;
  esac
fi

if ((${#PROFILE_SELECTED[@]} == 0)); then
  if ((PROFILE_SET == 0)) && [[ "$MODE" != "baseline" && "$MODE" != "measure" && "$MODE" != "all" ]]; then
    PROFILE_SELECTED=("${SELECTED[@]}")
  else
    PROFILE_SELECTED=("$PRIMARY")
  fi
fi
for wl in "${PROFILE_SELECTED[@]}"; do
  [[ -f "$BENCH_DIR/workloads/$wl.txt" ]] || die "unknown --profile-workload: $wl"
done

if ((ROUTES < 2)); then
  for wl in "${SELECTED[@]}" "${PROFILE_SELECTED[@]}"; do
    [[ "$wl" == "mixed" ]] && die "the mixed workload needs --routes >= 2 (it reads /page-001)"
  done
fi

# Server flags: raw --flags wins, otherwise map --cache/--wal to -x/-w.
if [[ -n "$FLAGS_OVERRIDE" ]]; then
  FLAGS_STR="$FLAGS_OVERRIDE"
else
  server_flags=()
  [[ "$CACHE" == "on" ]] && server_flags+=(-x)
  [[ "$WAL" == "on" ]] && server_flags+=(-w)
  FLAGS_STR="${server_flags[*]:-}"
fi

# Remote client setup. The server binds 0.0.0.0 when the client is elsewhere.
if bench_remote_enabled; then
  if [[ -z "$BENCH_TARGET_HOST" ]]; then
    BENCH_TARGET_HOST="$(hostname -I 2>/dev/null | awk '{print $1}' || true)"
    [[ -n "$BENCH_TARGET_HOST" ]] || \
      die "BENCH_HOST is set; export BENCH_TARGET_HOST to this server's address"
    warn "BENCH_TARGET_HOST not set; assuming $BENCH_TARGET_HOST"
  fi
  BENCH_BIND_HOST="${BENCH_BIND_HOST:-0.0.0.0}"
else
  BENCH_TARGET_HOST="${BENCH_TARGET_HOST:-127.0.0.1}"
  BENCH_BIND_HOST="${BENCH_BIND_HOST:-127.0.0.1}"
fi
export BENCH_TARGET_HOST BENCH_BIND_HOST

# Results directory.
if [[ -z "$RESULTS" ]]; then
  base="$RESULTS_DIR/$(utc_stamp)"
  RESULTS="$base"
  n=1
  while [[ -e "$RESULTS" ]]; do
    RESULTS="$base-$n"
    n=$((n + 1))
  done
fi

print_plan() {
  cat <<EOF
Bialet benchmark plan (DRY_RUN=1; nothing will execute)

  mode:            $MODE
  results:         $RESULTS
  binary:          $BINARY
  sites:           $SITES (ports $PORT_BASE..$((PORT_BASE + SITES - 1)))
  server flags:    ${FLAGS_STR:-(none)}
  cache:           $CACHE
  wal:             $WAL
  duration:        ${DURATION}s
  connections:     $CONNECTIONS
  repeats:         $REPEATS
  warmup:          ${WARMUP}s
  cooldown:        ${COOLDOWN}s
  load client:     ${BENCH_LOAD_IMPL}${BENCH_HOST:+ on $BENCH_HOST}
  target host:     $BENCH_TARGET_HOST (bind $BENCH_BIND_HOST)
  measurement:     ${SELECTED[*]}
  profile/attach:  ${PROFILE_SELECTED[*]}
  extra cflags:    ${EXTRA_CFLAGS:-}
  extra ldflags:   ${EXTRA_LDFLAGS:-}

Steps:
  1. $([[ "$NO_BUILD" == "1" ]] && echo "skip build" || { if [[ "$CLEAN_BUILD" == "1" ]]; then echo "make clean, then make -C $REPO_ROOT"; else echo "make -C $REPO_ROOT"; fi; })
  2. gen-sites.sh --count $SITES --routes $ROUTES --seed $SEED
  3. start-sites.sh --count $SITES --port-base $PORT_BASE --bind-host $BENCH_BIND_HOST --flags "$FLAGS_STR"
  4. preflight every workload, then ${WARMUP}s warm-up per workload
EOF
  if [[ "$MODE" == "baseline" || "$MODE" == "measure" || "$MODE" == "all" ]]; then
    cat <<EOF
  5. $REPEATS x ${DURATION}s load per workload: ${SELECTED[*]}
EOF
  fi
  if [[ "$MODE" == "measure" || "$MODE" == "all" ]]; then
    cat <<EOF
  6. perf stat window per workload (cycles, instructions, branches, cache)
EOF
  fi
  if [[ "$MODE" == "profile" || "$MODE" == "all" ]]; then
    cat <<EOF
  7. perf record -g --call-graph dwarf around: ${PROFILE_SELECTED[*]}
EOF
  fi
  if [[ "$MODE" == "alloc" || "$MODE" == "all" ]]; then
    cat <<EOF
  8. heaptrack attach around: ${PROFILE_SELECTED[*]}
EOF
  fi
  if [[ "$MODE" == "syscalls" || "$MODE" == "all" ]]; then
    cat <<EOF
  9. strace -c attach around: ${PROFILE_SELECTED[*]}
EOF
  fi
  cat <<EOF
 10. stop servers and RSS sampler; report.sh${COMPARE:+ --baseline $COMPARE}
EOF
}

if [[ "${DRY_RUN:-0}" == "1" ]]; then
  print_plan
  exit 0
fi

is_linux || die "this harness needs Linux (perf, heaptrack, strace, /proc). macOS cannot run it."

for wl in "${SELECTED[@]}" "${PROFILE_SELECTED[@]}"; do
  [[ -f "$BENCH_DIR/workloads/$wl.txt" ]] || die "missing workload file for $wl"
done

case "$MODE" in
  baseline|measure|profile|alloc|syscalls|all) ;;
  *) die "unknown mode: $MODE" ;;
esac

if [[ -d "$RESULTS" ]] && [[ -n "$(ls -A -- "$RESULTS" 2>/dev/null)" ]]; then
  die "results directory is not empty: $RESULTS (pick a new --results path)"
fi

ensure_dir "$RESULTS"
for dir in meta load perf profile alloc syscalls mem warmup logs; do
  ensure_dir "$RESULTS/$dir"
done
ensure_dir "$RUN_DIR"

# ---------------------------------------------------------------------------
# Metadata
# ---------------------------------------------------------------------------

record_config() {
  local file="$RESULTS/run.env"
  {
    printf 'mode=%s\n' "$MODE"
    printf 'utc=%s\n' "$(date -u +%FT%TZ)"
    printf 'sites=%s\n' "$SITES"
    printf 'routes=%s\n' "$ROUTES"
    printf 'seed=%s\n' "$SEED"
    printf 'port_base=%s\n' "$PORT_BASE"
    printf 'flags=%s\n' "$FLAGS_STR"
    printf 'cache=%s\n' "$CACHE"
    printf 'wal=%s\n' "$WAL"
    printf 'duration=%s\n' "$DURATION"
    printf 'connections=%s\n' "$CONNECTIONS"
    printf 'repeats=%s\n' "$REPEATS"
    printf 'warmup=%s\n' "$WARMUP"
    printf 'cooldown=%s\n' "$COOLDOWN"
    printf 'primary=%s\n' "$PRIMARY"
    printf 'workloads=%s\n' "${SELECTED[*]}"
    printf 'profile_workloads=%s\n' "${PROFILE_SELECTED[*]}"
    printf 'binary=%s\n' "$BINARY"
    printf 'binary_sha256=%s\n' "$(sha256_of "$BINARY")"
    printf 'extra_cflags=%s\n' "${EXTRA_CFLAGS:-}"
    printf 'extra_ldflags=%s\n' "${EXTRA_LDFLAGS:-}"
    printf 'bench_host=%s\n' "${BENCH_HOST:-}"
    printf 'bench_target_host=%s\n' "$BENCH_TARGET_HOST"
    printf 'bench_bind_host=%s\n' "$BENCH_BIND_HOST"
    printf 'load_impl=%s\n' "${LOAD_IMPL:-}"
    printf 'rss_hz=%s\n' "$RSS_HZ"
    printf 'git_head=%s\n' "$(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
    printf 'git_describe=%s\n' "$(git -C "$REPO_ROOT" describe --tags --always --dirty 2>/dev/null || echo unknown)"
  } >"$file"
}

collect_metadata() {
  local file="$RESULTS/meta/environment.txt"
  : >"$file"
  run_logged "$file" "date" date -u
  run_logged "$file" "uname" uname -a
  run_logged "$file" "nproc" nproc
  run_logged "$file" "free" free -h
  if have lscpu; then run_logged "$file" "lscpu" lscpu; fi
  run_logged "$file" "compiler" sh -c 'cc --version | head -n 2'
  run_logged "$file" "make" sh -c 'make --version | head -n 1'
  run_logged "$file" "python3" python3 --version
  if [[ -r /proc/sys/kernel/perf_event_paranoid ]]; then
    run_logged "$file" "perf_event_paranoid" cat /proc/sys/kernel/perf_event_paranoid
  fi
  if [[ -r /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]]; then
    run_logged "$file" "scaling_governor" cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
  fi
  if have cpupower; then
    run_logged "$file" "cpupower" cpupower frequency-info
  fi
  run_logged "$file" "git_head" git -C "$REPO_ROOT" rev-parse HEAD
  run_logged "$file" "git_status" git -C "$REPO_ROOT" status --porcelain
  run_logged "$file" "git_describe" git -C "$REPO_ROOT" describe --tags --always --dirty
  run_logged "$file" "binary" sh -c "ls -l '$BINARY'; sha256sum '$BINARY' 2>/dev/null || shasum -a 256 '$BINARY'"
  run_logged "$file" "extra_cflags" printf '%s\n' "${EXTRA_CFLAGS:-}"
  run_logged "$file" "extra_ldflags" printf '%s\n' "${EXTRA_LDFLAGS:-}"
}

# ---------------------------------------------------------------------------
# Build and generate
# ---------------------------------------------------------------------------

if ((NO_BUILD == 0)); then
  if ((CLEAN_BUILD == 1)); then
    info "make clean (removes build/ and every _db.sqlite3 under the tree)"
    make -C "$REPO_ROOT" clean 2>&1 | tee "$RESULTS/meta/build-clean.log"
  fi
  info "make -C $REPO_ROOT"
  make -C "$REPO_ROOT" \
    EXTRA_CFLAGS="${EXTRA_CFLAGS:-}" EXTRA_LDFLAGS="${EXTRA_LDFLAGS:-}" \
    2>&1 | tee "$RESULTS/meta/build.log"
fi

[[ -x "$BINARY" ]] || die "binary not executable: $BINARY (run make, or drop --no-build)"
BINARY="$(abs_path "$(dirname -- "$BINARY")")/$(basename -- "$BINARY")"

info "load client init"
load_init

record_config
collect_metadata

info "generating $SITES app(s)"
gen_args=(--count "$SITES" --routes "$ROUTES" --seed "$SEED")
((RESET_DB == 1)) && gen_args+=(--force)
bash "$BENCH_DIR/gen-sites.sh" "${gen_args[@]}" \
  >>"$RESULTS/meta/generate.log" 2>&1

# ---------------------------------------------------------------------------
# Servers
# ---------------------------------------------------------------------------

MANIFEST="$RESULTS/servers.tsv"
PIDS_FILE="$RUN_DIR/sites.pids"
RSS_PHASE_FILE="$RESULTS/mem/phase.txt"
RSS_CSV="$RESULTS/mem/rss.csv"
RSS_SAMPLER=""
STOPPED=0
BASE_URLS=()
SITE_CHILDREN=()

stop_stack() {
  ((STOPPED == 1)) && return 0
  STOPPED=1
  if [[ -n "$RSS_SAMPLER" ]] && kill -0 "$RSS_SAMPLER" 2>/dev/null; then
    info "stopping RSS sampler"
    stop_pid "$RSS_SAMPLER" 5
  fi
  bash "$BENCH_DIR/stop-sites.sh" --manifest "$MANIFEST" \
    --pids-file "$PIDS_FILE" --run-dir "$RUN_DIR" \
    >>"$RESULTS/logs/stop.log" 2>&1 || warn "stop-sites.sh reported a problem"
  printf 'stopped\n' >"$RSS_PHASE_FILE" 2>/dev/null || true
}

cleanup() {
  local rc=$?
  trap - EXIT INT TERM
  stop_stack
  exit "$rc"
}
trap cleanup EXIT INT TERM

start_args=(--count "$SITES" --port-base "$PORT_BASE" --apps-dir "$APPS_DIR"
            --binary "$BINARY" --bind-host "$BENCH_BIND_HOST" --flags "$FLAGS_STR"
            --log-dir "$RESULTS/logs" --manifest "$MANIFEST"
            --pids-file "$PIDS_FILE" --no-rss --wait 60)
bash "$BENCH_DIR/start-sites.sh" "${start_args[@]}"

while IFS=$'\t' read -r site port app supervisor child log; do
  [[ "$site" =~ ^# || -z "$site" ]] && continue
  BASE_URLS+=("http://$BENCH_TARGET_HOST:$port")
  SITE_CHILDREN+=("$child")
done <"$MANIFEST"
((${#BASE_URLS[@]} > 0)) || die "no servers found in $MANIFEST"
PRIMARY_CHILD="${SITE_CHILDREN[0]}"
PERF_PIDS="$(IFS=,; printf '%s' "${SITE_CHILDREN[*]}")"

printf 'idle\n' >"$RSS_PHASE_FILE"
bash "$BENCH_DIR/mem.sh" --manifest "$MANIFEST" --out "$RSS_CSV" \
  --hz "$RSS_HZ" --duration 0 --phase-file "$RSS_PHASE_FILE" \
  >"$RESULTS/mem/rss.log" 2>&1 &
RSS_SAMPLER=$!
info "RSS sampler pid $RSS_SAMPLER -> $RSS_CSV"

# ---------------------------------------------------------------------------
# Preflight and warm-up
# ---------------------------------------------------------------------------

for wl in "${SELECTED[@]}"; do
  preflight "$BENCH_DIR/workloads/$wl.txt" "${BASE_URLS[@]}"
done

if ((WARMUP > 0)); then
  printf 'warmup\n' >"$RSS_PHASE_FILE"
  for wl in "${SELECTED[@]}"; do
    workload_load "$BENCH_DIR/workloads/$wl.txt"
    mapfile -t urls < <(workload_url_list "$BENCH_DIR/workloads/$wl.txt" "${BASE_URLS[@]}")
    info "warm-up $wl (${WARMUP}s, 1 connection)"
    load_run "$RESULTS/warmup/$wl" "$WARMUP" 1 "${urls[@]}" >/dev/null
  done
fi
printf 'idle\n' >"$RSS_PHASE_FILE"

# ---------------------------------------------------------------------------
# Measurement
# ---------------------------------------------------------------------------

run_load_series() {
  local r wl file urls=()
  for ((r = 1; r <= REPEATS; r++)); do
    for wl in "${SELECTED[@]}"; do
      file="$BENCH_DIR/workloads/$wl.txt"
      workload_load "$file"
      mapfile -t urls < <(workload_url_list "$file" "${BASE_URLS[@]}")
      ensure_dir "$RESULTS/load/$wl"
      printf 'load-%s-r%s\n' "$wl" "$r" >"$RSS_PHASE_FILE"
      info "load $wl repeat $r/$REPEATS: ${DURATION}s x ${CONNECTIONS} -> ${#urls[@]} URL(s)"
      load_run "$RESULTS/load/$wl/r$r" "$DURATION" "$CONNECTIONS" \
        "${urls[@]}" >"$RESULTS/load/$wl/r$r.format"
    done
    printf 'idle\n' >"$RSS_PHASE_FILE"
    if ((r < REPEATS && COOLDOWN > 0)); then
      info "cooldown ${COOLDOWN}s"
      sleep "$COOLDOWN"
    fi
  done
}

run_perf_stats() {
  local wl file urls=() stat_pid
  have perf || { warn "perf missing; skipping perf stat"; return 0; }
  for wl in "${SELECTED[@]}"; do
    file="$BENCH_DIR/workloads/$wl.txt"
    workload_load "$file"
    mapfile -t urls < <(workload_url_list "$file" "${BASE_URLS[@]}")
    info "perf stat: $wl"
    perf stat -p "$PERF_PIDS" \
      -e cycles,instructions,branches,branch-misses,cache-references,cache-misses \
      -o "$RESULTS/perf/stat-$wl.txt" -- sleep "$DURATION" \
      >"$RESULTS/perf/stat-$wl.log" 2>&1 &
    stat_pid=$!
    sleep 0.3
    printf 'perfstat-%s\n' "$wl" >"$RSS_PHASE_FILE"
    load_run "$RESULTS/perf/load-$wl" "$DURATION" "$CONNECTIONS" \
      "${urls[@]}" >"$RESULTS/perf/load-$wl.format" || \
      warn "load run for perf stat failed: $wl"
    wait "$stat_pid" || warn "perf stat failed for $wl (perf_event_paranoid?)"
    printf 'idle\n' >"$RSS_PHASE_FILE"
  done
}

run_profiles() {
  local wl file urls=() args=()
  for wl in "${PROFILE_SELECTED[@]}"; do
    file="$BENCH_DIR/workloads/$wl.txt"
    workload_load "$file"
    mapfile -t urls < <(workload_url_list "$file" "${BASE_URLS[@]}")
    args=(--pid "$PRIMARY_CHILD" --out "$RESULTS/profile" --label "$wl"
          --duration "$DURATION" --connections "$CONNECTIONS")
    [[ "$WL_METHOD" != "GET" ]] && args+=(--method "$WL_METHOD")
    [[ -n "$WL_BODY" ]] && args+=(--data "$WL_BODY")
    [[ -n "$WL_CONTENT_TYPE" ]] && args+=(--content-type "$WL_CONTENT_TYPE")
    for u in "${urls[@]}"; do args+=(--url "$u"); done
    printf 'profile-%s\n' "$wl" >"$RSS_PHASE_FILE"
    if ! bash "$BENCH_DIR/profile.sh" "${args[@]}"; then
      warn "profiling failed for $wl"
      [[ "$MODE" == "profile" ]] && return 1
    fi
    printf 'idle\n' >"$RSS_PHASE_FILE"
  done
}

run_alloc() {
  local wl="${PROFILE_SELECTED[0]}" file urls=() attach_pid trace
  if ! have heaptrack || ! have heaptrack_print; then
    printf 'heaptrack or heaptrack_print missing; install-tools.sh installs both.\n' \
      >"$RESULTS/alloc/SKIPPED"
    warn "heaptrack missing; skipping allocation profile"
    [[ "$MODE" == "alloc" ]] && return 1
    return 0
  fi
  file="$BENCH_DIR/workloads/$wl.txt"
  workload_load "$file"
  mapfile -t urls < <(workload_url_list "$file" "${BASE_URLS[@]}")
  trace="$RESULTS/alloc/heaptrack.zst"
  info "heaptrack attach: $wl"
  heaptrack --pid "$PRIMARY_CHILD" --output "$trace" \
    >"$RESULTS/alloc/attach.log" 2>&1 &
  attach_pid=$!
  sleep 2
  if ! kill -0 "$attach_pid" 2>/dev/null; then
    warn "heaptrack attach exited early; see $RESULTS/alloc/attach.log"
    [[ "$MODE" == "alloc" ]] && return 1
    return 0
  fi
  printf 'alloc-%s\n' "$wl" >"$RSS_PHASE_FILE"
  load_run "$RESULTS/alloc/load" "$DURATION" "$CONNECTIONS" "${urls[@]}" \
    >"$RESULTS/alloc/load.format" || warn "load run for heaptrack failed"
  # SIGINT makes heaptrack flush its trace; the server keeps running.
  kill -INT "$attach_pid" 2>/dev/null || true
  local deadline=$((SECONDS + 30))
  while kill -0 "$attach_pid" 2>/dev/null && ((SECONDS < deadline)); do
    sleep 0.2
  done
  kill -TERM "$attach_pid" 2>/dev/null || true
  wait "$attach_pid" 2>/dev/null || true
  printf 'idle\n' >"$RSS_PHASE_FILE"

  if [[ -s "$trace" ]]; then
    info "heaptrack_print"
    heaptrack_print "$trace" >"$RESULTS/alloc/heaptrack.txt" 2>&1 || \
      warn "heaptrack_print failed; see $RESULTS/alloc/heaptrack.txt"
  else
    # Some heaptrack versions append .zst even when the name already has it.
    trace="$(find "$RESULTS/alloc" -maxdepth 1 -type f -name 'heaptrack*' \
      ! -name '*.log' ! -name '*.txt' -print -quit 2>/dev/null || true)"
    if [[ -n "$trace" && -s "$trace" ]]; then
      info "heaptrack_print ($trace)"
      heaptrack_print "$trace" >"$RESULTS/alloc/heaptrack.txt" 2>&1 || \
        warn "heaptrack_print failed; see $RESULTS/alloc/heaptrack.txt"
    else
      warn "no heaptrack trace written (attach may not be supported; see attach.log)"
      [[ "$MODE" == "alloc" ]] && return 1
    fi
  fi
}

run_syscalls() {
  local wl="${PROFILE_SELECTED[0]}" file urls=() strace_pid
  if ! have strace; then
    printf 'strace missing.\n' >"$RESULTS/syscalls/SKIPPED"
    warn "strace missing; skipping syscall profile"
    [[ "$MODE" == "syscalls" ]] && return 1
    return 0
  fi
  file="$BENCH_DIR/workloads/$wl.txt"
  workload_load "$file"
  mapfile -t urls < <(workload_url_list "$file" "${BASE_URLS[@]}")
  info "strace -c attach: $wl"
  strace -f -c -p "$PRIMARY_CHILD" -o "$RESULTS/syscalls/strace-c.txt" \
    >"$RESULTS/syscalls/strace.log" 2>&1 &
  strace_pid=$!
  sleep 1
  if ! kill -0 "$strace_pid" 2>/dev/null; then
    warn "strace attach failed; see $RESULTS/syscalls/strace.log"
    [[ "$MODE" == "syscalls" ]] && return 1
    return 0
  fi
  printf 'syscall-%s\n' "$wl" >"$RSS_PHASE_FILE"
  load_run "$RESULTS/syscalls/load" "$DURATION" "$CONNECTIONS" "${urls[@]}" \
    >"$RESULTS/syscalls/load.format" || warn "load run for strace failed"
  kill -INT "$strace_pid" 2>/dev/null || true
  wait "$strace_pid" 2>/dev/null || true
  printf 'idle\n' >"$RSS_PHASE_FILE"
  [[ -s "$RESULTS/syscalls/strace-c.txt" ]] || \
    warn "strace produced no summary; see $RESULTS/syscalls/strace.log"
}

case "$MODE" in
  baseline)
    run_load_series
    ;;
  measure)
    run_load_series
    run_perf_stats
    ;;
  profile)
    run_profiles
    ;;
  alloc)
    run_alloc
    ;;
  syscalls)
    run_syscalls
    ;;
  all)
    run_load_series
    run_perf_stats
    run_profiles || true
    run_alloc || true
    run_syscalls || true
    ;;
esac

stop_stack
info "servers stopped"

if ((NO_REPORT == 0)); then
  report_args=(--results "$RESULTS")
  [[ -n "$COMPARE" ]] && report_args+=(--baseline "$COMPARE")
  if ! bash "$BENCH_DIR/report.sh" "${report_args[@]}"; then
    warn "report.sh failed; raw results are in $RESULTS"
  fi
fi

printf '\nresults: %s\n' "$RESULTS"
printf 'run:     %s/run.sh %s\n' "$BENCH_DIR" "$MODE"
