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

# Tests - Request & Response
run_test "Get the URL parameter       " "get?foo=bar"     200 "bar"
run_test "Get the post parameter      " "post" "foo=bar"  200 "bar"
run_test "Get the route parameter     " "route/baz/qux"   200 "bazqux"
run_test "Redirection                 " "redirect"        302 ""
run_test "Forbid hidden file          " "_hidden"         403
run_test "This URL not exists         " "donotexists"     404
run_test "Check HTTP method           " "method-check"    200 "GET"
run_test "Response status codes       " "status-codes?code=404" 404 "not found"
run_test "Response status 201         " "status-codes?code=201" 201 "created"
run_test "Response headers            " "headers"         200 "headers-set"

# Tests - JSON & Parsing
run_test "JSON response               " "json"            200 '{"foo":"bar"}'
run_test "JSON parse and stringify    " "json-parse"      200 "Alice,30"
run_test "JSON edge cases             " "json-edge"       200 "all-passed"
run_test "Parse inline HTML strings   " "tags.wren"       200 "$(read_file "tags.html")"
run_test "Parsing error               " "parsing-error"   500
run_test "Markdown ordered list       " "markdown-ol"    200 "<ol>"

# Tests - Database
run_test "Database save and fetch     " "db"              200 "John Doe"
run_test "Query order by              " "query-order"     200 "item2,item3,item1"
run_test "Query val method            " "query-val"       200 "testvalue"
run_test "Query toNum method          " "query-tonum"     200 "50"

# Tests - HTTP & External
run_test "API call                    " "http"            200 "Adeel Solangi"
run_test "Third party modules         " "emoji"           200 "❤️"

# Tests - Date & Time
run_test "Date formatting             " "date"            200 "13/09/2024 15:45:30"

# Tests - Util functions
run_test "Util functions              " "util"            200 "true"

# Tests - Cookie & Session
run_test "Cookie set                  " "cookie?set=1"    200 "set"
run_test "Session get empty           " "session?get=1"   200 "empty"

# Tests - Config
run_test "Config operations           " "config"          200 "test_value,42,true"

# Tests - String & List Extensions
run_test "String extensions           " "string-ext"      200 "hello,WORLD"
run_test "List extensions             " "list-ext"        200 "1,null"

# Tests - CORS
run_test "CORS enabled                " "cors"            200 "cors"

finish
print_summary >&2

exit $?
