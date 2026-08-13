#!/bin/bash

# Counters
total_tests=0
passed_tests=0
failed_tests=0
skipped_tests=0

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Record a test as skipped (not run) rather than failed. Used when TARGET_EXEC
# is "-" and a capability the test needs (a local binary, an echo server, a
# filesystem shared with the target server) isn't available. Skips don't
# affect total_tests or the exit code.
skip_test() {
    description=$1
    reason=$2

    echo -e -n "$description\t"
    echo -e "${YELLOW}SKIP${NC}"
    echo -e -n "\t$reason\n"
    skipped_tests=$((skipped_tests + 1))
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

    total_tests=$((total_tests + 1))
    echo -e -n "$description\t"
    response=$(curl -s -o /dev/null -w "%{http_code}" "http://$HOST:$PORT/$url_path")
    body=$(curl -s "http://$HOST:$PORT/$url_path")

    if [[ "$response" -ne "$expected_status" ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
        echo -e -n "\tExpected: ${expected_status}\tActual: $response\n"
        return 1
    fi

    if [[ -n "$expected_body" && "$body" != *"$expected_body"* ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
        echo -e -n "\tExpected: ${expected_body}\tActual: $body\n"
        return 1
    fi

    echo -e "${GREEN}PASS${NC}"
    passed_tests=$((passed_tests + 1))
}

# Function to run and assert POST requests
test_post() {
    description=$1
    url_path=$2
    post_data=$3
    expected_status=$4
    expected_body=${5:-}

    total_tests=$((total_tests + 1))
    echo -e -n "$description\t"
    response=$(curl -s -o /dev/null -w "%{http_code}" -d "$post_data" "http://$HOST:$PORT/$url_path")
    body=$(curl -s -d "$post_data" "http://$HOST:$PORT/$url_path")

    if [[ "$response" -ne "$expected_status" ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
        echo -e -n "\tExpected: ${expected_status}\tActual: $response\n"
        return 1
    fi

    if [[ -n "$expected_body" && "$body" != *"$expected_body"* ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
        echo -e -n "\tExpected: ${expected_body}\tActual: $body\n"
        return 1
    fi

    echo -e "${GREEN}PASS${NC}"
    passed_tests=$((passed_tests + 1))
}

# Test wrapper function to determine GET or POST based on arguments
run_test() {
    if [[ "$#" -eq 4 ]]; then
        test_get "$1" "$2" "$3" "$4"
    elif [[ "$#" -eq 3 ]]; then
        test_get "$1" "$2" "$3"
    elif [[ "$#" -eq 5 ]]; then
        test_post "$1" "$2" "$3" "$4" "$5"
    elif [[ "$#" -eq 4 ]]; then
        test_post "$1" "$2" "$3" "$4"
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

    total_tests=$((total_tests + 1))
    echo -e -n "$description\t"
    response=$(curl -s -o /dev/null -w "%{http_code}" -u "$user:$pass" "http://$HOST:$PORT/$url_path")
    body=$(curl -s -u "$user:$pass" "http://$HOST:$PORT/$url_path")

    if [[ "$response" -ne "$expected_status" ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
        echo -e -n "\tExpected: ${expected_status}\tActual: $response\n"
        return 1
    fi

    if [[ -n "$expected_body" && "$body" != *"$expected_body"* ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
        echo -e -n "\tExpected: ${expected_body}\tActual: $body\n"
        return 1
    fi

    echo -e "${GREEN}PASS${NC}"
    passed_tests=$((passed_tests + 1))
}

# Function to run and assert an arbitrary HTTP method (status + header + body)
test_method() {
    description=$1
    method=$2
    url_path=$3
    expected_status=$4
    expected_header=${5:-}
    expected_body=${6:-}

    total_tests=$((total_tests + 1))
    echo -e -n "$description\t"
    response=$(curl -s -o /dev/null -w "%{http_code}" -X "$method" "http://$HOST:$PORT/$url_path")
    headers=$(curl -s -D- -o /dev/null -X "$method" "http://$HOST:$PORT/$url_path")
    body=$(curl -s -X "$method" "http://$HOST:$PORT/$url_path")

    if [[ "$response" -ne "$expected_status" ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
        echo -e -n "\tExpected: ${expected_status}\tActual: $response\n"
        return 1
    fi

    if [[ -n "$expected_header" && "$headers" != *"$expected_header"* ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
        echo -e -n "\tExpected header: ${expected_header}\tActual headers: $headers\n"
        return 1
    fi

    if [[ -n "$expected_body" && "$body" != *"$expected_body"* ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
        echo -e -n "\tExpected: ${expected_body}\tActual: $body\n"
        return 1
    fi

    echo -e "${GREEN}PASS${NC}"
    passed_tests=$((passed_tests + 1))
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

    total_tests=$((total_tests + 1))
    echo -e -n "$description\t"
    filepath="$(dirname "$0")/$file"

    if [[ -n "$root_path" ]]; then
        "$TARGET_EXEC" -t "$filepath" "$root_path" > /dev/null 2>&1
    else
        "$TARGET_EXEC" -t "$filepath" > /dev/null 2>&1
    fi
    actual_exit=$?

    if [[ "$actual_exit" -ne "$expected_exit" ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
        echo -e -n "\tExpected exit: ${expected_exit}\tActual: $actual_exit\n"
        return 1
    fi

    echo -e "${GREEN}PASS${NC}"
    passed_tests=$((passed_tests + 1))
}

test_syntax_msg() {
    description=$1
    file=$2
    expected_exit=$3
    expected_msg=$4
    root_path=${5:-}

    total_tests=$((total_tests + 1))
    echo -e -n "$description\t"
    filepath="$(dirname "$0")/$file"

    if [[ -n "$root_path" ]]; then
        output=$("$TARGET_EXEC" -t "$filepath" "$root_path" 2>&1)
    else
        output=$("$TARGET_EXEC" -t "$filepath" 2>&1)
    fi
    actual_exit=$?

    if [[ "$actual_exit" -ne "$expected_exit" ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
        echo -e -n "\tExpected exit: ${expected_exit}\tActual: $actual_exit\n"
        return 1
    fi

    if [[ -n "$expected_msg" && "$output" != *"$expected_msg"* ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
        echo -e -n "\tExpected message: $expected_msg\tActual: $output\n"
        return 1
    fi

    echo -e "${GREEN}PASS${NC}"
    passed_tests=$((passed_tests + 1))
}

# Function to print the final result
print_summary() {
    echo -e "\nSummary:\n"
    echo -e "Total Tests: $total_tests"
    echo -e "${GREEN}Passed Tests: $passed_tests${NC}"
    echo -e "${RED}Failed Tests: $failed_tests${NC}"
    echo -e "${YELLOW}Skipped Tests: $skipped_tests${NC}"

    if [ "$failed_tests" -ne 0 ]; then
        return 1
    else
        return 0
    fi
}

# Start server
echo -e "🚲Run tests with ${BLUE}$TARGET_EXEC${NC} server on ${BLUE}$HOST:$PORT${NC}\n"

# Start server
if [[ "$TARGET_EXEC" != "-" ]]; then
  echo -e -n "${BLUE}Start server process...\t"
  $TARGET_EXEC -h $HOST -p $PORT -l /tmp/tests.log $(dirname "$0") > /dev/null 2>&1 &
  disown

  # Wait for server to start
  sleep 3

  # Check if server is running
  PID=$(pgrep -f -o "$TARGET_EXEC -h $HOST -p $PORT -l /tmp/tests.log")

  if [[ ${#PID} != 0 ]]
  then
    echo -e "${GREEN}OK${NC}"
  else
    echo -e "${RED}FAIL${NC}"
    exit 1
  fi
fi

# Check if server port is up
echo -e -n "${BLUE}Server port is up...\t"
# Try connecting to the port (works on both Linux and macOS)
(bash -c "echo >/dev/tcp/$HOST/$PORT" 2>/dev/null &
PID=$!
sleep 5
kill $PID 2>/dev/null)
bash -c "echo >/dev/tcp/$HOST/$PORT" &> /dev/null
if [[ $? == 0 ]]
then
  echo -e "${GREEN}OK${NC}"
else
  echo -e "${RED}FAIL${NC}"
  exit 1
fi

echo -e "\n${BLUE}Run tests\n---------\n\e[0m"

finish() {
  if [[ "$TARGET_EXEC" != "-" ]]; then
    echo -e -n "\n${BLUE}Stop server process...\t"
    # kill all
    pgrep -f "$TARGET_EXEC -h $HOST -p $PORT -l /tmp/tests.log" 2>/dev/null | xargs -I {} kill -9 {} 2>/dev/null
    echo -e " ${GREEN}OK${NC}"
  fi
}
