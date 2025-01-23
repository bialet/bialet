#!/bin/bash
#
# Test runner
#
# Usage: ./run.sh [executable] [host] [port]
# Use "-" to not start the server process
#

# Parameters
TARGET_EXEC="${1:-./build/bialet}"
HOST="${2:-127.0.0.1}"
PORT="${3:-7001}"

source "$(dirname "$0")/util.sh"

# Tests
run_test "Get the URL parameter       " "get?foo=bar"     200 "bar"
run_test "Get the post parameter      " "post" "foo=bar"  200 "bar"
run_test "Get the route parameter     " "route/baz/qux"   200 "bazqux"
run_test "Redirection                 " "redirect"        302 ""
run_test "Forbid hidden file          " "_hidden"         403
run_test "This URL not exists         " "donotexists"     404
run_test "Parsing error               " "parsing-error"   500
run_test "API call                    " "http"            200 "Adeel Solangi"
run_test "JSON response               " "json"            200 '{"foo":"bar"}'
run_test "Database save and fetch     " "db"              200 "John Doe"
run_test "Parse inline HTML strings   " "tags.wren"       200 "$(read_file "tags.html")"
run_test "Third party modules         " "emoji"           200 "❤️"

finish
print_summary >&2

exit $?
