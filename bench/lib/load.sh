#!/usr/bin/env bash
# Load-generation helpers shared by run.sh, profile.sh and mem.sh.
#
# Client selection:
#   BENCH_LOAD_IMPL=auto|oha|wrk|python   (default auto)
#   BENCH_HOST=<ssh target>               run the client on another machine
#   BENCH_TARGET_HOST=<host>              server address as seen by the client
#
# oha is preferred: its JSON output is stable and it accepts several URLs,
# which is how the "mixed" workload and the multi-site scenario are driven.
# wrk is the fallback, with vectors set through the environment. The Python
# fallback keeps the harness usable with nothing but python3.

if [[ -n "${BENCH_LOAD_SH:-}" ]]; then
  return 0
fi
BENCH_LOAD_SH=1

# shellcheck source=common.sh
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/common.sh"

: "${BENCH_LOAD_IMPL:=auto}"
: "${BENCH_HOST:=}"
: "${BENCH_TARGET_HOST:=}"

LOAD_IMPL=""
LOAD_LAST_FORMAT=""
OHA_NO_TUI=0
REMOTE_WRK_SCRIPT=""

bench_remote_enabled() { [[ -n "$BENCH_HOST" ]]; }

# Quote an argv array for a remote POSIX shell. bash's %q is understood by the
# remote shell in practice (it only quotes with backslashes and single quotes).
shell_join() {
  local out="" arg quoted
  for arg in "$@"; do
    printf -v quoted '%q' "$arg"
    out+="$quoted "
  done
  printf '%s' "${out% }"
}

# client_probe COMMAND... -> prints the command's stdout, locally or over ssh.
client_probe() {
  if bench_remote_enabled; then
    ssh -o BatchMode=yes -o ConnectTimeout=10 "$BENCH_HOST" "$(shell_join "$@")"
  else
    "$@"
  fi
}

# run_client OUT ERR COMMAND... -> run on the client host, redirect its streams.
run_client() {
  local out="$1" err="$2"
  shift 2
  if bench_remote_enabled; then
    ssh -o BatchMode=yes -o ConnectTimeout=10 "$BENCH_HOST" "$(shell_join "$@")" \
      >"$out" 2>"$err"
  else
    "$@" >"$out" 2>"$err"
  fi
}

# Copy the wrk Lua request script to the remote client once per process.
prepare_remote_wrk() {
  [[ -n "$REMOTE_WRK_SCRIPT" ]] && return 0
  local path="/tmp/bialet-bench-wrk-$$.lua"
  ssh -o BatchMode=yes -o ConnectTimeout=10 "$BENCH_HOST" \
    "cat > $(printf '%q' "$path")" <"$BENCH_DIR/lib/wrk-request.lua" \
    || die "could not copy the wrk script to $BENCH_HOST"
  REMOTE_WRK_SCRIPT="$path"
}

load_init() {
  local requested="$BENCH_LOAD_IMPL" found=""

  if bench_remote_enabled; then
    [[ -n "$BENCH_TARGET_HOST" ]] || \
      die "BENCH_HOST is set; also set BENCH_TARGET_HOST to the server address reachable from the client"
  fi
  if [[ "$requested" == "auto" ]]; then
    if bench_remote_enabled; then
      # The remote box is the recommended place for the client, so probe it.
      found="$(client_probe sh -c 'for c in oha wrk; do command -v "$c" >/dev/null 2>&1 && { echo "$c"; exit 0; }; done; exit 1' 2>/dev/null || true)"
      [[ -z "$found" ]] && die "no load client on $BENCH_HOST: install oha (preferred) or wrk there"
      LOAD_IMPL="$found"
    elif have oha; then
      LOAD_IMPL="oha"
    elif have wrk; then
      LOAD_IMPL="wrk"
    elif have python3; then
      LOAD_IMPL="python"
    else
      die "no load generator found: install oha, wrk or python3"
    fi
  else
    LOAD_IMPL="$requested"
    case "$LOAD_IMPL" in
      oha)
        client_probe sh -c 'command -v oha >/dev/null 2>&1' \
          || die "BENCH_LOAD_IMPL=oha but oha is not available on the client"
        ;;
      wrk)
        client_probe sh -c 'command -v wrk >/dev/null 2>&1' \
          || die "BENCH_LOAD_IMPL=wrk but wrk is not available on the client"
        ;;
      python)
        bench_remote_enabled && die "the python fallback runs on the server host only; use oha or wrk over BENCH_HOST"
        have python3 || die "BENCH_LOAD_IMPL=python but python3 is missing"
        ;;
      *) die "unknown BENCH_LOAD_IMPL: $LOAD_IMPL (use auto, oha, wrk or python)" ;;
    esac
  fi

  if [[ "$LOAD_IMPL" == "oha" ]] && \
     client_probe sh -c 'oha --help 2>&1 | grep -q -- --no-tui'; then
    OHA_NO_TUI=1
  fi
  if [[ "$LOAD_IMPL" == "wrk" ]]; then
    bench_remote_enabled && prepare_remote_wrk
  fi

  client_probe sh -c 'command -v curl >/dev/null 2>&1' \
    || die "curl is required for the preflight request (on the load-client host)"

  if bench_remote_enabled; then
    info "load client: $LOAD_IMPL on $BENCH_HOST -> $BENCH_TARGET_HOST"
  else
    info "load client: $LOAD_IMPL (local)"
  fi
}

# ---------------------------------------------------------------------------
# Workload files
#
# One key=value per line. Recognised keys:
#   name, path, method, status, body, content_type, kind=mixed, paths=a,b,c
# `paths` is only used when kind=mixed. Comments start with #.
# ---------------------------------------------------------------------------

WL_NAME="" WL_PATH="/" WL_METHOD="GET" WL_STATUS="200" WL_BODY=""
WL_CONTENT_TYPE="" WL_KIND="single" WL_PATHS=""

workload_load() {
  local file="$1" line key value
  [[ -r "$file" ]] || die "workload not found: $file"
  WL_NAME="$(basename -- "$file" .txt)"
  WL_PATH="/"
  WL_METHOD="GET"
  WL_STATUS="200"
  WL_BODY=""
  WL_CONTENT_TYPE=""
  WL_KIND="single"
  WL_PATHS=""
  while IFS= read -r line || [[ -n "$line" ]]; do
    line="${line%$'\r'}"
    [[ "$line" =~ ^[[:space:]]*$ ]] && continue
    [[ "$line" =~ ^[[:space:]]*# ]] && continue
    [[ "$line" == *=* ]] || die "$file: expected key=value, got: $line"
    key="${line%%=*}"
    value="${line#*=}"
    key="${key#"${key%%[![:space:]]*}"}"
    key="${key%"${key##*[![:space:]]}"}"
    case "$key" in
      name) WL_NAME="$value" ;;
      path) WL_PATH="$value" ;;
      method) WL_METHOD="${value^^}" ;;
      status) WL_STATUS="$value" ;;
      body) WL_BODY="$value" ;;
      content_type) WL_CONTENT_TYPE="$value" ;;
      kind) WL_KIND="$value" ;;
      paths) WL_PATHS="$value" ;;
      *) die "$file: unknown workload key: $key" ;;
    esac
  done <"$file"

  is_uint "$WL_STATUS" || die "$file: status must be a number"
  [[ "$WL_PATH" == /* ]] || die "$file: path must start with /"
  if [[ "$WL_KIND" == "mixed" ]]; then
    [[ -n "$WL_PATHS" ]] || die "$file: kind=mixed needs paths=a,b,c"
  elif [[ "$WL_KIND" != "single" ]]; then
    die "$file: kind must be single or mixed"
  fi
}

# workload_url_list FILE BASE_URL... -> one absolute URL per line.
workload_url_list() {
  local file="$1"
  shift
  local bases=("$@") base path
  workload_load "$file"
  for base in "${bases[@]}"; do
    base="${base%/}"
    if [[ "$WL_KIND" == "mixed" ]]; then
      local IFS=,
      # shellcheck disable=SC2206
      local paths=($WL_PATHS)
      unset IFS
      for path in "${paths[@]}"; do
        printf '%s%s\n' "$base" "$path"
      done
    else
      printf '%s%s\n' "$base" "$WL_PATH"
    fi
  done
}

# preflight FILE BASE_URL... - one real request per URL, expected status check.
preflight() {
  local file="$1"
  shift
  local urls=() url status extra=() cmd=()
  mapfile -t urls < <(workload_url_list "$file" "$@")
  workload_load "$file"
  for url in "${urls[@]}"; do
    extra=()
    if [[ "$WL_METHOD" != "GET" ]]; then
      extra+=(-X "$WL_METHOD")
    fi
    if [[ -n "$WL_BODY" ]]; then
      extra+=(--data-binary "$WL_BODY")
    fi
    if [[ -n "$WL_CONTENT_TYPE" ]]; then
      extra+=(-H "Content-Type: $WL_CONTENT_TYPE")
    fi
    cmd=(curl -sS --max-time 15 -o /dev/null -w '%{http_code}' "${extra[@]}" "$url")
    status="$(client_probe "${cmd[@]}" 2>/dev/null || true)"
    if [[ "$status" != "$WL_STATUS" ]]; then
      die "preflight failed for $WL_NAME: $WL_METHOD $url -> ${status:-no response} (expected $WL_STATUS). Check the generated app and the server log."
    fi
  done
  info "preflight ok: $WL_NAME ($WL_METHOD, expected $WL_STATUS)"
}

# ---------------------------------------------------------------------------
# Running a load
#
# load_run OUTBASE DURATION_SECONDS CONNECTIONS URL...
# Writes OUTBASE.json (oha/python) or OUTBASE.txt (wrk). Sets LOAD_LAST_FORMAT.
# ---------------------------------------------------------------------------

load_run() {
  local outbase="$1" duration="$2" connections="$3"
  shift 3
  local urls=("$@")
  local out err fmt cmd=() path_list="" url_path
  is_uint "$duration" || die "duration must be whole seconds, got: $duration"
  ((duration >= 1)) || die "duration must be at least 1 second"
  is_uint "$connections" || die "connections must be a number"
  ((connections >= 1)) || die "connections must be at least 1"
  ((${#urls[@]} > 0)) || die "load_run needs at least one URL"

  case "$LOAD_IMPL" in
    oha)
      fmt="json"
      out="$outbase.json" err="$outbase.err"
      cmd=(oha)
      if [[ "$OHA_NO_TUI" == "1" ]]; then
        cmd+=(--no-tui)
      fi
      cmd+=(-j -z "${duration}s" -c "$connections")
      if [[ "$WL_METHOD" != "GET" ]]; then
        cmd+=(-m "$WL_METHOD")
      fi
      if [[ -n "$WL_BODY" ]]; then
        cmd+=(-d "$WL_BODY")
      fi
      if [[ -n "$WL_CONTENT_TYPE" ]]; then
        cmd+=(-H "Content-Type: $WL_CONTENT_TYPE")
      fi
      cmd+=("${urls[@]}")
      ;;
    wrk)
      fmt="text"
      out="$outbase.txt" err="$outbase.err"
      local origin="" url_origin
      for url_path in "${urls[@]}"; do
        url_origin="${url_path#*://}"
        url_origin="${url_origin%%/*}"
        if [[ -z "$origin" ]]; then
          origin="$url_origin"
        elif [[ "$url_origin" != "$origin" ]]; then
          die "wrk cannot drive multiple origins ($origin vs $url_origin); install oha for multi-site or mixed-across-sites runs"
        fi
      done
      for url_path in "${urls[@]}"; do
        # Strip scheme and authority, keep path+query. A bare origin becomes /.
        url_path="${url_path#http://}"
        url_path="${url_path#https://}"
        if [[ "$url_path" == */* ]]; then
          url_path="/${url_path#*/}"
        else
          url_path="/"
        fi
        path_list+="${path_list:+,}$url_path"
      done
      local wrk_script="$BENCH_DIR/lib/wrk-request.lua"
      bench_remote_enabled && wrk_script="$REMOTE_WRK_SCRIPT"
      local threads="${BENCH_WRK_THREADS:-1}"
      is_uint "$threads" && ((threads >= 1)) || threads=1
      if ((connections < threads)); then
        threads="$connections"
      fi
      cmd=(env "BENCH_METHOD=$WL_METHOD" "BENCH_BODY=$WL_BODY" \
           "BENCH_CTYPE=$WL_CONTENT_TYPE" "BENCH_PATHS=$path_list" \
           wrk -t "$threads" -c "$connections" -d "${duration}s" --latency \
           -s "$wrk_script" "${urls[0]}")
      ;;
    python)
      fmt="json"
      out="$outbase.json" err="$outbase.err"
      cmd=(python3 "$BENCH_DIR/lib/load.py" --duration "$duration" \
           --connections "$connections" --method "$WL_METHOD")
      if [[ -n "$WL_BODY" ]]; then
        cmd+=(--body "$WL_BODY")
      fi
      if [[ -n "$WL_CONTENT_TYPE" ]]; then
        cmd+=(--content-type "$WL_CONTENT_TYPE")
      fi
      cmd+=("${urls[@]}")
      ;;
    *) die "load client not initialised" ;;
  esac

  LOAD_LAST_FORMAT="$fmt"
  if ! run_client "$out" "$err" "${cmd[@]}"; then
    # oha exits non-zero when any request failed (connection refused, RST).
    # Keep the JSON so the report can show the failure rate instead of hiding
    # the run behind a shell error.
    if [[ ! -s "$out" ]]; then
      die "load client failed: ${cmd[*]} (see $err)"
    fi
    warn "load client reported failures; results kept in $out"
  fi
  printf '%s\n' "$fmt"
}
