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
ECHO_PORT="${4:-7100}"

source "$(dirname "$0")/util.sh"

# Start echo server used by the Http.* method tests (POST/PUT/DELETE targets)
if [[ "$TARGET_EXEC" != "-" ]]; then
  $TARGET_EXEC -h $HOST -p $ECHO_PORT -l /tmp/tests-echo.log "$(dirname "$0")/echo" > /dev/null 2>&1 &
  disown
  sleep 1
fi

# Tests - Syntax validation (-t flag)
if [[ "$TARGET_EXEC" != "-" ]]; then
  test_syntax "Syntax validation passes       " "syntax_ok.wren"  0
  test_syntax "Syntax validation fails        " "syntax_err.wren" 1
  test_syntax "Syntax import passes w/ root   " "syntax_import_ok.wren"  0 "$(dirname "$0")"
  test_syntax "Syntax import fail w/ root    " "syntax_import_err.wren" 1 "$(dirname "$0")"
  test_syntax "Deep nested tags pass         " "syntax_ok_deepnest.wren" 0
  test_syntax "Multiline interp passes       " "syntax_ok_interp_multiline.wren" 0
  test_syntax "Nested same tag rejected      " "syntax_err_nested.wren" 1
  test_syntax "Leading op rejected           " "syntax_err_interp_leading_op.wren" 1
  test_syntax "Uppercase tag rejected        " "syntax_err_uppercase.wren" 1
fi

# Tests - Request & Response
run_test "Get the URL parameter       " "get?foo=bar"     200 "bar"
run_test "Get the post parameter      " "post" "foo=bar"  200 "bar"
run_test "Get the route parameter     " "route/baz/qux"   200 "bazqux"
run_test "Redirection                 " "redirect"        302 ""
run_test "Forbid hidden file          " "_hidden"         403
run_test "This URL not exists         " "donotexists"     404
run_test "Custom 404 error page       " "donotexists"     404 "custom-404-page"
run_test "Check HTTP method           " "method-check"    200 "GET"
run_test "Response status codes       " "status-codes?code=404" 404 "not found"
run_test "Response status 201         " "status-codes?code=201" 201 "created"
run_test "Response headers            " "headers"         200 "headers-set"

# Tests - Request & Response (extended coverage)
run_test "Request uri and query alias " "request-meta?foo=bar" 200 "get|/request-meta|bar"
run_test "Request body and header     " "request-meta" "foo=bar" 200 "form|/request-meta|foo=bar|application/x-www-form-urlencoded"

# Regression for the quadratic List.join/urlDecode DoS (DOS-001/DOS-002): a
# ~1MB newline-delimited POST used to stall the single-threaded server for
# minutes (O(n^2) copies). It must now complete promptly and keep the HTTP
# layer responsive. The payload goes through a file because it exceeds curl's
# command-line argument limit.
total_tests=$((total_tests + 1))
echo -e -n "Large POST body no hang      \t"
dos_payload="$(mktemp)"
if command -v python3 >/dev/null 2>&1; then
  python3 -c "open('$dos_payload', 'w').write('a\n' * 500000)"
else
  head -c 500000 /dev/zero | tr '\0' 'a' | sed 's/./&\n/g' > "$dos_payload"
fi
dos_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 20 \
  --data-binary "@$dos_payload" "http://$HOST:$PORT/post")
rm -f "$dos_payload"
if [[ "$dos_code" != "200" ]]; then
  echo -e "${RED}FAIL${NC}"
  failed_tests=$((failed_tests + 1))
  echo -e -n "\tExpected: 200\tActual: $dos_code (quadratic hang or timeout)\n"
else
  echo -e "${GREEN}PASS${NC}"
  passed_tests=$((passed_tests + 1))
fi

run_test "Response page escapes title " "response-page" 200 "Page&lt;title&gt;"
run_test "Response page escapes msg   " "response-page" 200 "Hello &amp; welcome"
run_test "Response out buffer         " "response-out"  200 "out:[first"
run_test "Response headers getter     " "response-headers" 200 "custom:true|cookie:true"
run_test "Response forbidden          " "response-errors?forbidden=1" 403
run_test "Response notFound custom    " "response-errors?notfound=1" 404 "custom-404-page"
run_test "Cookie delete and defaults  " "cookie-delete" 200 "present:empty|fallback:fallback-value|missing:null"
run_test "Session meta                " "session-meta" 200 "sidLen:40|default:BIALETSESSID|renamed:MYTESTCOOKIE|got:meta_value"
run_test "Directory index resolution  " "route"           200 "route-index"
run_test "Directory index trailing /  " "route/"          200 "route-index"
run_test "Wren extension is optional  " "method-check.wren" 200 "GET"

test_method "PUT method                " "PUT"    "method-check" 200 "" "PUT"
test_method "DELETE method             " "DELETE" "method-check" 200 "" "DELETE"
test_method "PATCH method              " "PATCH"  "method-check" 200 "" "PATCH"
test_method "OPTIONS CORS preflight    " "OPTIONS" "cors" 204 "Access-Control-Allow-Origin: *" ""
test_auth   "Login without credentials " "login-check" "" "" 401
test_auth   "Login invalid credentials " "login-check" "admin" "wrong" 401
test_auth   "Login valid credentials   " "login-check" "admin" "secret" 200 "authenticated"

# Tests - JSON & Parsing
run_test "JSON response               " "json"            200 '{"foo":"bar"}'
run_test "JSON parse and stringify    " "json-parse"      200 "Alice,30"
run_test "JSON edge cases             " "json-edge"       200 "all-passed"
run_test "Parse inline HTML strings   " "tags.wren"       200 "$(read_file "tags.html")"
run_test "Parsing error               " "parsing-error"   500
run_test "Custom 500 error page       " "parsing-error"   500 "custom-500-page"
# Tests - Markdown
run_test "Markdown ordered list       " "markdown-ol"    200 "<ol>"
run_test "Markdown file rendering     " "md-file"        200 "<h1>Test Heading</h1>"

# Tests - Database
run_test "Database save and fetch     " "db"              200 "John Doe"
run_test "Query order by              " "query-order"     200 "item2,item3,item1"
run_test "Query val method            " "query-val"       200 "testvalue"
run_test "Query toNum method          " "query-tonum"     200 "50"
run_test "Query save method           " "save"            200 "hello updated 2024-09-13 10:30:00"
run_test "Query toBool method         " "db-tobool"       200 "true,false,true,true,true"
run_test "Query to(Class) mapping     " "db-to-class"     200 "alpha:10,beta:20"
run_test "Query RETURNING clause      " "db-returning"    200 "5,5,5"
run_test "Db save delete migrate      " "db-more"         200 "inserted:"

# Tests - HTTP & External
run_test "API call                    " "http"            200 "Adeel Solangi"
run_test "Third party modules         " "emoji"           200 "❤️"
run_test "Http POST PUT DELETE        " "http-methods?target=http://$HOST:$ECHO_PORT" 200 "POST|PUT|DELETE|GET|GET"

# Tests - Date & Time
run_test "Date formatting             " "date"            200 "13/09/2024 15:45:30"
run_test "Date now and components     " "date-now"        200 "true"
run_test "Date comparison operators   " "date-compare"    200 "true,true,true,true,true,true"
run_test "Date parse from string      " "date-parse"      200 "2024-12-25 14:30:45,UTC"
run_test "Date constructors and get   " "date-more"       200 "dow:5|doy:366|woy:37|diff:86400|fmt:2024-09-13"

# Tests - Util functions
run_test "Util functions              " "util"            200 "true"
run_test "Util encoding and helpers   " "util-more"       200 "hex:255|hexLower:26|toHex:FF|lpad:007|rev:cba"

# Tests - Cookie & Session
run_test "Cookie set                  " "cookie?set=1"    200 "set"
run_test "Session get empty           " "session?get=1"   200 "empty"
run_test "Session CSRF token render   " "csrf"            200 "_bialet_csrf"
run_test "Session CSRF check fail     " "csrf" ""         200 "fail"

# Multi-form CSRF: every form on a page must carry a token that validates.
# Uses a cookie jar so the POST hits the same session as the GET.
total_tests=$((total_tests + 1))
echo -e -n "Session CSRF multi-form        \t"
csrf_cookie=/tmp/bialet-csrf-cookies.txt
csrf_page=$(curl -s -c "$csrf_cookie" "http://$HOST:$PORT/csrf-multi")
csrf_tokens=$(printf "%s" "$csrf_page" | grep -o 'name="_bialet_csrf" value="[^"]*"' | sed 's/name="_bialet_csrf" value="//; s/"//')
csrf_first=$(printf "%s" "$csrf_tokens" | head -1)
csrf_last=$(printf "%s" "$csrf_tokens" | tail -1)
csrf_first_post=$(curl -s -b "$csrf_cookie" -d "_bialet_csrf=$csrf_first" "http://$HOST:$PORT/csrf-multi")
csrf_last_post=$(curl -s -b "$csrf_cookie" -d "_bialet_csrf=$csrf_last" "http://$HOST:$PORT/csrf-multi")
rm -f "$csrf_cookie"
if [[ -n "$csrf_first" && "$csrf_first" == "$csrf_last" \
      && "$csrf_first_post" == "OK" && "$csrf_last_post" == "OK" ]]; then
  echo -e "${GREEN}PASS${NC}"
  passed_tests=$((passed_tests + 1))
else
  echo -e "${RED}FAIL${NC}"
  failed_tests=$((failed_tests + 1))
  echo -e "\tExpected a single shared token with POST OK. Tokens: '$csrf_tokens' | first POST: '$csrf_first_post' | last POST: '$csrf_last_post'"
fi

# Tests - Config
run_test "Config operations           " "config"          200 "test_value,42,true,false,true"
run_test "Config json and delete      " "config-json"     200 "a:1|b:2|raw:{\"b\":2}|gone:true"

# Tests - String & List Extensions
run_test "String extensions           " "string-ext"      200 "hello,WORLD"
run_test "List extensions             " "list-ext"        200 "1,null"
run_test "String trim edge cases      " "string-trim"     200 "hello,trim,no-whitespace,,123,trimleft,trimright,hello"
run_test "More extensions             " "extensions-more" 200 "toNum:42,null|map:Alice|list:2,A,B|take:1,2"

# Tests - File handling
run_test "File create get destroy     " "file-more"       200 "tempBlocksGet:true|saveRestores:true|destroy:true"

# Tests - safe method on basic types
run_test "Safe method basic types     " "safe"            200 "true,42,{html: &lt;b&gt;bold&lt;/b&gt;}"

# Tests - Random
run_test "Random sample               " "randomsample"    200 "true,true,true"

# Tests - CORS
run_test "CORS enabled                " "cors"            200 "cors"

if [[ "$TARGET_EXEC" != "-" ]]; then
  pgrep -f "$TARGET_EXEC -h $HOST -p $ECHO_PORT -l /tmp/tests-echo.log" 2>/dev/null | xargs -I {} kill -9 {} 2>/dev/null
fi

finish
print_summary >&2

exit $?
