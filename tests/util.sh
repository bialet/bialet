#!/bin/bash

# Counters
total_tests=0
passed_tests=0
failed_tests=0

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

# Function to run and assert GET requests
test_get() {
    description=$1
    url_path=$2
    expected_status=$3
    expected_body=${4:-}

    total_tests=$((total_tests + 1))
    echo -e -n "$description\t"
    response=$(curl -s -o /dev/null -w "%{http_code}" "http://localhost:7000/$url_path")
    body=$(curl -s "http://localhost:7000/$url_path")

    if [[ "$response" -ne "$expected_status" ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
        return 1
    fi

    if [[ -n "$expected_body" && "$body" != *"$expected_body"* ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
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
    response=$(curl -s -o /dev/null -w "%{http_code}" -d "$post_data" "http://localhost:7000/$url_path")
    body=$(curl -s -d "$post_data" "http://localhost:7000/$url_path")

    if [[ "$response" -ne "$expected_status" ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
        return 1
    fi

    if [[ -n "$expected_body" && "$body" != *"$expected_body"* ]]; then
        echo -e "${RED}FAIL${NC}"
        failed_tests=$((failed_tests + 1))
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

# Function to print the final result
print_summary() {
    echo -e "\nSummary:"
    echo -e "Total Tests: $total_tests"
    echo -e "${GREEN}Passed Tests: $passed_tests${NC}"
    echo -e "${RED}Failed Tests: $failed_tests${NC}"

    if [ "$failed_tests" -ne 0 ]; then
        return 1
    else
        return 0
    fi
}
