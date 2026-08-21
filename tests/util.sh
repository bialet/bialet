#!/bin/bash

# Counters
total_tests=0
passed_tests=0
failed_tests=0
skipped_tests=0

# Buffered "line|description|detail" triples for -q mode: failures are held
# until the whole run is done, then printed as ### FAIL blocks after the one
# summary line (crux-style), instead of interleaving with the run.
failures=()

# Wall-clock start, used by print_summary's "in Nms" duration. python3 (already
# a soft dependency elsewhere in this script) gives sub-second precision;
# without it, bash's SECONDS builtin falls back to whole-second precision.
if command -v python3 >/dev/null 2>&1; then
  test_start_time=$(python3 -c 'import time; print(time.time())')
else
  test_start_time=""
  SECONDS=0
fi

# Color codes. Empty in -q so no escape sequences reach piped/CI logs.
if [[ "${QUIET:-0}" == 1 ]]; then
  RED=''
  GREEN=''
  BLUE=''
  YELLOW=''
  NC=''
else
  RED='\033[0;31m'
  GREEN='\033[0;32m'
  BLUE='\033[0;34m'
  YELLOW='\033[1;33m'
  NC='\033[0m' # No Color
fi

# Millisecond wall-clock, used for per-test durations. bash's EPOCHREALTIME
# (5.0+) is cheap and precise; python3 (already a soft dependency elsewhere in
# this script) is the portable fallback; SECONDS drops to whole-second
# precision as a last resort.
now_ms() {
    if [[ -n "${EPOCHREALTIME:-}" ]]; then
        # EPOCHREALTIME uses the locale's decimal separator (a comma under some
        # locales), so normalize before splitting.
        local t=${EPOCHREALTIME//,/.}
        local s=${t%.*} us=${t#*.}
        echo $((s * 1000 + 10#$us / 1000))
    elif command -v python3 >/dev/null 2>&1; then
        python3 -c 'import time; print(int(time.time() * 1000))'
    else
        echo $((SECONDS * 1000))
    fi
}

# Wait for a TCP port to accept connections, polling instead of sleeping a fixed
# amount. Servers start in tens of milliseconds, so a fixed 2-3s sleep wasted
# almost all of it on every run. Returns 0 when the port comes up, 1 on timeout.
# python3 (already a soft dependency elsewhere) gives sub-second polling; the
# fallback polls once a second.
wait_port() {
    local host=$1 port=$2 timeout=${3:-10}
    if command -v python3 >/dev/null 2>&1; then
        python3 -c "
import socket, sys, time
host, port, deadline = sys.argv[1], int(sys.argv[2]), time.time() + float(sys.argv[3])
while time.time() < deadline:
    try:
        socket.create_connection((host, port), 0.25).close()
        sys.exit(0)
    except OSError:
        time.sleep(0.05)
sys.exit(1)
" "$host" "$port" "$timeout"
    else
        local deadline=$(( $(now_ms) + timeout * 1000 ))
        while [[ $(now_ms) -lt deadline ]]; do
            if (exec 3<>"/dev/tcp/$host/$port") 2>/dev/null; then
                return 0
            fi
            sleep 1
        done
        return 1
    fi
}

# Wait for an HTTP endpoint to return a given status code (default 200), again
# polling rather than sleeping a fixed amount. Used where the server's port may
# be up before the app is ready (e.g. bialet dev enables livereload after
# binding). Returns 0 on success, 1 on timeout.
wait_http() {
    local url=$1 expected=${2:-200} timeout=${3:-10}
    local deadline=$(( $(now_ms) + timeout * 1000 ))
    local code=""
    while [[ $(now_ms) -lt deadline ]]; do
        code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 2 "$url" 2>/dev/null)
        [[ "$code" == "$expected" ]] && return 0
        sleep 1
    done
    return 1
}

# Per-test start timestamp, set at the entry of every test helper and before
# each inline test block in run.sh; report_result() reads it for the (Nms).
_test_start_ms=$(now_ms)

# Records one test's outcome. In normal mode, prints a colored symbol line (and
# a detail line under a failure). In -q, only failures are kept (buffered for
# the end-of-run FAIL blocks); passes print nothing.
report_result() {
    local description=$1 line=$2 ok=$3 detail=${4:-}
    local elapsed_ms=$(( $(now_ms) - _test_start_ms ))

    total_tests=$((total_tests + 1))
    if [[ "$ok" -eq 0 ]]; then
        passed_tests=$((passed_tests + 1))
        if [[ "${QUIET:-0}" != 1 ]]; then
            echo -e "  ${GREEN}✓${NC} $description (${elapsed_ms}ms)"
        fi
    else
        failed_tests=$((failed_tests + 1))
        if [[ "${QUIET:-0}" == 1 ]]; then
            failures+=("$line|$description|$detail")
        else
            echo -e "  ${RED}✗${NC} $description (${elapsed_ms}ms)"
            [[ -n "$detail" ]] && echo -e "      $detail"
        fi
    fi
}

# Record a test as skipped (not run) rather than failed. Used when TARGET_EXEC
# is "-" and a capability the test needs (a local binary, an echo server, a
# filesystem shared with the target server) isn't available. Skips don't
# affect total_tests or the exit code, and (like a pass) print nothing in -q.
skip_test() {
    description=$1
    reason=$2

    skipped_tests=$((skipped_tests + 1))
    if [[ "${QUIET:-0}" != 1 ]]; then
        echo -e "  ${YELLOW}○${NC} $description"
        echo -e "      $reason"
    fi
}

# Non-fatal TCP reachability probe with a timeout, used to detect optional
# capabilities (e.g. an echo server on a given port) without hanging or
# aborting the run. Distinct from the blocking startup port-wait below, which
# is used to confirm the server this script itself started has come up.
check_port() {
    local host=$1 port=$2 timeout=${3:-2}
    (exec 3<>"/dev/tcp/$host/$port") 2>/dev/null &
    local pid=$!
    (sleep "$timeout" && kill "$pid" 2>/dev/null) &
    local killer=$!
    wait "$pid" 2>/dev/null
    local status=$?
    kill "$killer" 2>/dev/null
    wait "$killer" 2>/dev/null
    return $status
}

# Function to run and assert GET requests
test_get() {
    description=$1
    url_path=$2
    expected_status=$3
    expected_body=${4:-}
    line=$5
    _test_start_ms=$(now_ms)

    response=$(curl -s -o /dev/null -w "%{http_code}" "http://$HOST:$PORT/$url_path")
    body=$(curl -s "http://$HOST:$PORT/$url_path")

    if [[ "$response" -ne "$expected_status" ]]; then
        report_result "$description" "$line" 1 "Expected: ${expected_status}\tActual: $response"
        return 1
    fi

    if [[ -n "$expected_body" && "$body" != *"$expected_body"* ]]; then
        report_result "$description" "$line" 1 "Expected: ${expected_body}\tActual: $body"
        return 1
    fi

    report_result "$description" "$line" 0
}

# Function to run and assert POST requests
test_post() {
    description=$1
    url_path=$2
    post_data=$3
    expected_status=$4
    expected_body=${5:-}
    line=$6
    _test_start_ms=$(now_ms)

    response=$(curl -s -o /dev/null -w "%{http_code}" -d "$post_data" "http://$HOST:$PORT/$url_path")
    body=$(curl -s -d "$post_data" "http://$HOST:$PORT/$url_path")

    if [[ "$response" -ne "$expected_status" ]]; then
        report_result "$description" "$line" 1 "Expected: ${expected_status}\tActual: $response"
        return 1
    fi

    if [[ -n "$expected_body" && "$body" != *"$expected_body"* ]]; then
        report_result "$description" "$line" 1 "Expected: ${expected_body}\tActual: $body"
        return 1
    fi

    report_result "$description" "$line" 0
}

# Test wrapper function to determine GET or POST based on arguments. Computes
# the call site here (its own direct caller, i.e. the line in run.sh) since
# test_get/test_post are invoked through this wrapper rather than directly.
run_test() {
    local line=${BASH_LINENO[0]}
    if [[ "$#" -eq 5 ]]; then
        test_post "$1" "$2" "$3" "$4" "$5" "$line"
    elif [[ "$#" -eq 4 ]]; then
        test_get "$1" "$2" "$3" "$4" "$line"
    elif [[ "$#" -eq 3 ]]; then
        test_get "$1" "$2" "$3" "" "$line"
    fi
}

# Function to run and assert a request using HTTP basic auth
test_auth() {
    description=$1
    url_path=$2
    user=$3
    pass=$4
    expected_status=$5
    expected_body=${6:-}
    line=${BASH_LINENO[0]}
    _test_start_ms=$(now_ms)

    response=$(curl -s -o /dev/null -w "%{http_code}" -u "$user:$pass" "http://$HOST:$PORT/$url_path")
    body=$(curl -s -u "$user:$pass" "http://$HOST:$PORT/$url_path")

    if [[ "$response" -ne "$expected_status" ]]; then
        report_result "$description" "$line" 1 "Expected: ${expected_status}\tActual: $response"
        return 1
    fi

    if [[ -n "$expected_body" && "$body" != *"$expected_body"* ]]; then
        report_result "$description" "$line" 1 "Expected: ${expected_body}\tActual: $body"
        return 1
    fi

    report_result "$description" "$line" 0
}

# Function to run and assert an arbitrary HTTP method (status + header + body)
test_method() {
    description=$1
    method=$2
    url_path=$3
    expected_status=$4
    expected_header=${5:-}
    expected_body=${6:-}
    line=${BASH_LINENO[0]}
    _test_start_ms=$(now_ms)

    response=$(curl -s -o /dev/null -w "%{http_code}" -X "$method" "http://$HOST:$PORT/$url_path")
    headers=$(curl -s -D- -o /dev/null -X "$method" "http://$HOST:$PORT/$url_path")
    body=$(curl -s -X "$method" "http://$HOST:$PORT/$url_path")

    if [[ "$response" -ne "$expected_status" ]]; then
        report_result "$description" "$line" 1 "Expected: ${expected_status}\tActual: $response"
        return 1
    fi

    if [[ -n "$expected_header" && "$headers" != *"$expected_header"* ]]; then
        report_result "$description" "$line" 1 "Expected header: ${expected_header}\tActual headers: $headers"
        return 1
    fi

    if [[ -n "$expected_body" && "$body" != *"$expected_body"* ]]; then
        report_result "$description" "$line" 1 "Expected: ${expected_body}\tActual: $body"
        return 1
    fi

    report_result "$description" "$line" 0
}

read_file() {
    file_path=$1
    cat "$(dirname "$0")/$file_path"
}

test_syntax() {
    description=$1
    file=$2
    expected_exit=$3
    root_path=${4:-}
    line=${BASH_LINENO[0]}
    _test_start_ms=$(now_ms)

    filepath="$(dirname "$0")/$file"

    if [[ -n "$root_path" ]]; then
        "$TARGET_EXEC" -t "$filepath" "$root_path" > /dev/null 2>&1
    else
        "$TARGET_EXEC" -t "$filepath" > /dev/null 2>&1
    fi
    actual_exit=$?

    if [[ "$actual_exit" -ne "$expected_exit" ]]; then
        report_result "$description" "$line" 1 "Expected exit: ${expected_exit}\tActual: $actual_exit"
        return 1
    fi

    report_result "$description" "$line" 0
}

test_syntax_msg() {
    description=$1
    file=$2
    expected_exit=$3
    expected_msg=$4
    root_path=${5:-}
    line=${BASH_LINENO[0]}
    _test_start_ms=$(now_ms)

    filepath="$(dirname "$0")/$file"

    if [[ -n "$root_path" ]]; then
        output=$("$TARGET_EXEC" -t "$filepath" "$root_path" 2>&1)
    else
        output=$("$TARGET_EXEC" -t "$filepath" 2>&1)
    fi
    actual_exit=$?

    if [[ "$actual_exit" -ne "$expected_exit" ]]; then
        report_result "$description" "$line" 1 "Expected exit: ${expected_exit}\tActual: $actual_exit"
        return 1
    fi

    if [[ -n "$expected_msg" && "$output" != *"$expected_msg"* ]]; then
        report_result "$description" "$line" 1 "Expected message: $expected_msg\tActual: $output"
        return 1
    fi

    report_result "$description" "$line" 0
}

# Function to print the final result
print_summary() {
    local elapsed
    if [[ -n "$test_start_time" ]] && command -v python3 >/dev/null 2>&1; then
        elapsed=$(python3 -c "import time; print(int((time.time() - $test_start_time) * 1000))")
    else
        elapsed=$((SECONDS * 1000))
    fi

    if [[ "${QUIET:-0}" == 1 ]]; then
        echo "$failed_tests of $total_tests tests failed in ${elapsed}ms"
        local entry f_line f_desc f_detail
        for entry in "${failures[@]}"; do
            IFS='|' read -r f_line f_desc f_detail <<< "$entry"
            echo
            echo "### FAIL run.sh:$f_line - $f_desc"
            [[ -n "$f_detail" ]] && echo -e "$f_detail"
        done
    else
        echo -e "\nSummary:\n"
        echo -e "Total Tests: $total_tests"
        echo -e "${GREEN}Passed Tests: $passed_tests${NC}"
        echo -e "${RED}Failed Tests: $failed_tests${NC}"
        echo -e "${YELLOW}Skipped Tests: $skipped_tests${NC}"
        echo -e "Elapsed Time: ${elapsed}ms"
    fi

    if [ "$failed_tests" -ne 0 ]; then
        return 1
    else
        return 0
    fi
}

if [[ "${QUIET:-0}" != 1 ]]; then
  echo -e "🚲Run tests with ${BLUE}$TARGET_EXEC${NC} server on ${BLUE}$HOST:$PORT${NC}\n"
fi

# Start server
if [[ "$TARGET_EXEC" != "-" ]]; then
  [[ "${QUIET:-0}" != 1 ]] && echo -e -n "${BLUE}Start server process...\t"
  $TARGET_EXEC -h $HOST -p $PORT -l /tmp/tests.log $(dirname "$0") > /dev/null 2>&1 &
  disown

  # Wait for server to start (poll the port instead of a fixed 3s sleep)
  wait_port "$HOST" "$PORT" 10

  # Check if server is running
  PID=$(pgrep -f -o "$TARGET_EXEC -h $HOST -p $PORT -l /tmp/tests.log")

  if [[ ${#PID} != 0 ]]
  then
    [[ "${QUIET:-0}" != 1 ]] && echo -e "${GREEN}OK${NC}"
  else
    echo -e "${RED}FAIL${NC}: server process did not start"
    exit 1
  fi
fi

# Check if server port is up
[[ "${QUIET:-0}" != 1 ]] && echo -e -n "${BLUE}Server port is up...\t"
# Try connecting to the port (works on both Linux and macOS)
(bash -c "echo >/dev/tcp/$HOST/$PORT" 2>/dev/null &
PID=$!
sleep 5
kill $PID 2>/dev/null)
bash -c "echo >/dev/tcp/$HOST/$PORT" &> /dev/null
if [[ $? == 0 ]]
then
  [[ "${QUIET:-0}" != 1 ]] && echo -e "${GREEN}OK${NC}"
else
  echo -e "${RED}FAIL${NC}: server port never came up"
  exit 1
fi

if [[ "${QUIET:-0}" != 1 ]]; then
  echo -e "\n${BLUE}Run tests\n---------\n\e[0m"
fi

finish() {
  if [[ "$TARGET_EXEC" != "-" ]]; then
    [[ "${QUIET:-0}" != 1 ]] && echo -e -n "\n${BLUE}Stop server process...\t"
    # kill all
    pgrep -f "$TARGET_EXEC -h $HOST -p $PORT -l /tmp/tests.log" 2>/dev/null | xargs -I {} kill -9 {} 2>/dev/null
    [[ "${QUIET:-0}" != 1 ]] && echo -e " ${GREEN}OK${NC}"
  fi
}
