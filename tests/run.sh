#!/bin/bash
#
# Test runner
#
# Usage: ./run.sh [-q] [executable] [host] [port]
# Use "-" to not start the server process
# -q: quiet mode -- no colors, minimal output (crux-style FAIL summary only)
#

# Pull -q out of the argument list wherever it appears; everything else stays
# positional (executable, host, port, ...) exactly as before.
QUIET=0
args=()
for arg in "$@"; do
  if [[ "$arg" == "-q" ]]; then
    QUIET=1
  else
    args+=("$arg")
  fi
done
export QUIET

# Parameters
TARGET_EXEC="${args[0]:-./build/bialet}"
HOST="${args[1]:-127.0.0.1}"
PORT="${args[2]:-7001}"
ECHO_PORT="${args[3]:-7100}"
SHOW_ERRORS_PORT="${args[4]:-7101}"
DEV_PORT="${args[5]:-7102}"
CRON_PORT="${args[6]:-7103}"

source "$(dirname "$0")/util.sh"

# Start echo server used by the Http.* method tests (POST/PUT/DELETE targets)
if [[ "$TARGET_EXEC" != "-" ]]; then
  $TARGET_EXEC -h $HOST -p $ECHO_PORT -l /tmp/tests-echo.log "$(dirname "$0")/echo" > /dev/null 2>&1 &
  disown
  wait_port "$HOST" "$ECHO_PORT" 10
fi

# Capability detection for TARGET_EXEC "-" (no local binary, testing against
# an already-running server). Rather than hard-failing or silently skipping
# whole test groups, probe what's actually available and run as much as
# possible. This is scoped to "-" only: with a real TARGET_EXEC, behavior is
# unchanged (both flags stay true, no probing, no extra latency).
ECHO_REACHABLE=1
FS_SHARED=1

if [[ "$TARGET_EXEC" == "-" ]]; then
  # Some other bialet instance rooted at tests/echo may already be listening
  # on $ECHO_PORT (e.g. started by hand). Probe instead of assuming.
  check_port "$HOST" "$ECHO_PORT" 2 || ECHO_REACHABLE=0

  # The symlink-bypass regression below needs to plant a file in this tests/
  # directory and have the *target server* see it - only true if the server
  # is reading this exact filesystem. Round-trip a random marker file
  # through it to find out.
  fs_probe_name="fs-share-probe-$$.html"
  fs_probe_path="$(dirname "$0")/$fs_probe_name"
  fs_probe_token="probe-$RANDOM$RANDOM"
  printf '%s' "$fs_probe_token" > "$fs_probe_path" 2>/dev/null
  fs_probe_body=$(curl -s --max-time 5 "http://$HOST:$PORT/$fs_probe_name")
  rm -f "$fs_probe_path"
  [[ "$fs_probe_body" == "$fs_probe_token" ]] || FS_SHARED=0
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
  test_syntax_msg "Invalid tag name clear error   " "syntax_err_invalid_tag_upper.wren" 1 "Invalid tag name: must be lowercase alphanumeric + hyphens"
  test_syntax_msg "Invalid tag name clear error   " "syntax_err_invalid_tag_underscore.wren" 1 "Invalid tag name: must be lowercase alphanumeric + hyphens"
else
  skip_test "Syntax validation (11 checks)  " "requires local binary access (-t flag)"
fi

# Tests - Module imports
# Regression: a relative import ("./x") inside a file that is itself only
# ever reached transitively (entry.wren -> ./sub/lib -> ./helper) must
# resolve against the importing file's own directory, not the entry file's.
run_test "Nested relative import         " "import-nested/entry" 200 "hi from helper"

# Tests - Request & Response
run_test "Get the URL parameter       " "get?foo=bar"     200 "bar"
run_test "Get the post parameter      " "post" "foo=bar"  200 "bar"
run_test "Get the route parameter     " "route/baz/qux"   200 "bazqux"
# Regression: dynamic segment sharing a letter prefix with the directory name
# (e.g. dir "reservar", segment "reunion-30min" both start with "re") must not
# be truncated by the route-prefix stripping logic.
run_test "Route param w/ dir overlap   " "reservar/reunion-30min" 200 "reunion-30min"
run_test "Redirection                 " "redirect"        302 ""
run_test "Forbid hidden file          " "_hidden"         403
# Regression: a planted sub/_route.wren -> ../_db.sqlite3 symlink must not
# bypass the private-file rule. The _route.wren directory search must not
# follow symlinks, and the resolved-path check is only waived for a real
# _route.wren basename. The probe returns 404 (no _route.wren found), never
# the leaked database bytes.
if [[ "$FS_SHARED" == 1 ]]; then
  route_symlink_line=$LINENO
  _test_start_ms=$(now_ms)
  route_symlink_dir="$(dirname "$0")/sub"
  route_symlink="$route_symlink_dir/_route.wren"
  rm -rf "$route_symlink_dir"
  route_symlink_code=""
  route_symlink_body=""
  if mkdir -p "$route_symlink_dir" && ln -s "../_db.sqlite3" "$route_symlink"; then
    route_symlink_code=$(curl -s -o /dev/null -w "%{http_code}" \
      "http://$HOST:$PORT/sub/does-not-exist")
    route_symlink_body=$(curl -s "http://$HOST:$PORT/sub/does-not-exist")
  fi
  rm -rf "$route_symlink_dir"
  if [[ "$route_symlink_code" == "404" && "$route_symlink_body" != "SQLite format 3"* ]]; then
    report_result "_route.wren symlink no bypass" "$route_symlink_line" 0
  else
    report_result "_route.wren symlink no bypass" "$route_symlink_line" 1 \
      "Expected 404, no DB bytes. Got code:$route_symlink_code body:'${route_symlink_body:0:40}'"
  fi
else
  skip_test "_route.wren symlink no bypass" "server filesystem is not shared with this script"
fi
run_test "This URL not exists         " "donotexists"     404
run_test "Custom 404 error page       " "donotexists"     404 "custom-404-page"
run_test "Check HTTP method           " "method-check"    200 "GET"
run_test "Response status codes       " "status-codes?code=404" 404 "not found"
run_test "Response status 201         " "status-codes?code=201" 201 "created"
run_test "Response headers            " "headers"         200 "headers-set"

# Tests - Request & Response (extended coverage)
run_test "Request uri and query alias " "request-meta?foo=bar" 200 "get|/request-meta|bar"
run_test "Request body and header     " "request-meta" "foo=bar" 200 "form|/request-meta|foo=bar|application/x-www-form-urlencoded"

# Regression for the quadratic List.join/urlDecode DoS (DOS-001/DOS-002) and
# the memory limit. A large newline-delimited POST used to stall the
# single-threaded server for minutes (O(n^2) copies) and, on Linux, crash the
# fork()ed child when Wren body parsing exceeded RLIMIT_AS. The request-body
# cap (derived from the soft memory limit) now bounds that work: a body at/below
# the cap must complete promptly with 200, and an oversized body must be
# rejected promptly with 413 - never hang or crash. Payloads go through files
# because they exceed curl's command-line argument limit.
large_post_line=$LINENO
_test_start_ms=$(now_ms)
under_payload="$(mktemp)"
over_payload="$(mktemp)"
if command -v python3 >/dev/null 2>&1; then
  python3 -c "open('$under_payload', 'w').write('a\n' * 30000)"  # 60KB, under the cap
  python3 -c "open('$over_payload', 'w').write('a\n' * 500000)"  # 1MB, over the cap
else
  head -c 30000 /dev/zero | tr '\0' 'a' | sed 's/./&\n/g' > "$under_payload"
  head -c 500000 /dev/zero | tr '\0' 'a' | sed 's/./&\n/g' > "$over_payload"
fi
under_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 20 \
  --data-binary "@$under_payload" "http://$HOST:$PORT/post")
over_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 20 \
  --data-binary "@$over_payload" "http://$HOST:$PORT/post")
alive_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 \
  "http://$HOST:$PORT/get?foo=bar")
rm -f "$under_payload" "$over_payload"
if [[ "$under_code" == "200" && "$over_code" == "413" && "$alive_code" == "200" ]]; then
  report_result "Large POST body no hang" "$large_post_line" 0
else
  report_result "Large POST body no hang" "$large_post_line" 1 \
    "Expected 200 under cap, 413 over cap, server alive. Got under:$under_code over:$over_code alive:$alive_code"
fi

# Custom 413 error page: like 404/500, an oversized body is served the app's
# own 413.html (or 413.wren) page via custom_error.
custom_413_line=$LINENO
_test_start_ms=$(now_ms)
over_payload="$(mktemp)"
if command -v python3 >/dev/null 2>&1; then
  python3 -c "open('$over_payload', 'w').write('a\n' * 500000)"
else
  head -c 500000 /dev/zero | tr '\0' 'a' | sed 's/./&\n/g' > "$over_payload"
fi
over_page=$(curl -s --max-time 20 \
  --data-binary "@$over_payload" "http://$HOST:$PORT/post")
rm -f "$over_payload"
if [[ "$over_page" == *"custom-413-page"* ]]; then
  report_result "Custom 413 error page" "$custom_413_line" 0
else
  report_result "Custom 413 error page" "$custom_413_line" 1 \
    "Expected the custom 413 page. Got: '$over_page'"
fi

run_test "Response page escapes title " "response-page" 200 "Page&lt;title&gt;"
run_test "Response page escapes msg   " "response-page" 200 "Hello &amp; welcome"
run_test "Response out buffer         " "response-out"  200 "out:[first"
run_test "Response out HTML literal   " "response-out-html" 200 "<p>hi</p>"
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
run_test "Auto-escape interpolation   " "escape.wren"     200 "$(read_file "escape.html")"
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
run_test "Query non-primitive params  " "query-param-stringify" 200 "node-name|tail|node-name"
run_test "Query to(Class) mapping     " "db-to-class"     200 "alpha:10,beta:20"
run_test "Query RETURNING clause      " "db-returning"    200 "5,5,5"
run_test "Db save delete migrate      " "db-more"         200 "inserted:"
run_test "NULL column maps to null     " "db-null"         200 "literal:true|bound:true"

# Tests - HTTP & External
run_test "API call                    " "http"            200 "Adeel Solangi"
run_test "Third party modules         " "emoji"           200 "❤️"
if [[ "$ECHO_REACHABLE" == 1 ]]; then
  run_test "Http POST PUT DELETE        " "http-methods?target=http://$HOST:$ECHO_PORT" 200 "POST|PUT|DELETE|GET|GET"
  run_test "Http client headers/auth/json " "http-client?target=http://$HOST:$ECHO_PORT" 200 "hello|Basic YWRtaW46c2VjcmV0|v|null"
  run_test "Http bearer token option    " "http-options?which=token&target=http://$HOST:$ECHO_PORT" 200 "Bearer secret-token"
  run_test "Http form-encoded option    " "http-options?which=form&target=http://$HOST:$ECHO_PORT" 200 "v1=hello world"
  run_test "Http cookie jar round-trip  " "http-options?which=cookie&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "session=abc123"
  run_test "Http cookie jar host scope  " "http-options?which=cookie-scope&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "set|no-cookie"
  run_test "Http cookie jar path scope  " "http-options?which=cookie-path&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "set|no-cookie"
  run_test "Http cookie jar domain scope" "http-options?which=cookie-domain&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "set|no-cookie"
  run_test "Http cookie jar secure flag " "http-options?which=cookie-secure&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "set|no-cookie"
  run_test "Http cookie jar expire      " "http-options?which=cookie-expire&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "set|no-cookie"
  run_test "Http cookie jar override    " "http-options?which=cookie-override&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "set|set|session=second"
  run_test "Http shortcuts never crash   " "http-error-handling?target=http://$HOST:$ECHO_PORT" 200 \
    "down:true|downError:true|downHasMessage:true|missing:true|missingStatus:404|missingError:false"
  run_test "Http cookie jar manual hdr  " "http-options?which=cookie-manual&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "set|manual=1"
  run_test "Http query-string builder   " "http-options?which=query&target=http://$HOST:$ECHO_PORT" 200 "hi there"
  run_test "Http timeout + error message" "http-options?which=timeout&target=http://$HOST:$ECHO_PORT" 200 "false|error-present"
else
  echo_skip_reason="echo server not reachable at $HOST:$ECHO_PORT"
  skip_test "Http POST PUT DELETE        " "$echo_skip_reason"
  skip_test "Http client headers/auth/json " "$echo_skip_reason"
  skip_test "Http bearer token option    " "$echo_skip_reason"
  skip_test "Http form-encoded option    " "$echo_skip_reason"
  skip_test "Http cookie jar round-trip  " "$echo_skip_reason"
  skip_test "Http cookie jar host scope  " "$echo_skip_reason"
  skip_test "Http cookie jar path scope  " "$echo_skip_reason"
  skip_test "Http cookie jar domain scope" "$echo_skip_reason"
  skip_test "Http cookie jar secure flag " "$echo_skip_reason"
  skip_test "Http cookie jar expire      " "$echo_skip_reason"
  skip_test "Http cookie jar override    " "$echo_skip_reason"
  skip_test "Http cookie jar manual hdr  " "$echo_skip_reason"
  skip_test "Http query-string builder   " "$echo_skip_reason"
  skip_test "Http timeout + error message" "$echo_skip_reason"
fi

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
csrf_multi_line=$LINENO
_test_start_ms=$(now_ms)
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
  report_result "Session CSRF multi-form" "$csrf_multi_line" 0
else
  report_result "Session CSRF multi-form" "$csrf_multi_line" 1 \
    "Expected a single shared token with POST OK. Tokens: '$csrf_tokens' | first POST: '$csrf_first_post' | last POST: '$csrf_last_post'"
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

# Tests - BIALET_SHOW_ERRORS
# With the flag enabled, a compile error must serve the error text instead of
# the generic 500 page. Uses a dedicated app dir + server instance (own DB, own
# migration that enables the flag) so it doesn't disturb the main test app.
if [[ "$TARGET_EXEC" != "-" ]]; then
  show_errors_line=$LINENO
  _test_start_ms=$(now_ms)
  show_root="$(dirname "$0")/show-errors"
  "$TARGET_EXEC" -h "$HOST" -p "$SHOW_ERRORS_PORT" -l /tmp/tests-show-errors.log \
    "$show_root" > /dev/null 2>&1 &
  disown
  wait_port "$HOST" "$SHOW_ERRORS_PORT" 10
  show_code=$(curl -s -o /dev/null -w "%{http_code}" \
    "http://$HOST:$SHOW_ERRORS_PORT/broken")
  show_body=$(curl -s "http://$HOST:$SHOW_ERRORS_PORT/broken")
  pgrep -f "$TARGET_EXEC -h $HOST -p $SHOW_ERRORS_PORT -l /tmp/tests-show-errors.log" \
    2>/dev/null | xargs -I {} kill -9 {} 2>/dev/null
  if [[ "$show_code" == "500" && "$show_body" == *"Compilation Error"* \
        && "$show_body" != *"Oops! Something broke"* ]]; then
    report_result "Show errors on compile error" "$show_errors_line" 0
  else
    report_result "Show errors on compile error" "$show_errors_line" 1 \
      "Expected 500 with 'Compilation Error', no generic 500 page. Got code:$show_code body:'$show_body'"
  fi
else
  skip_test "Show errors on compile error" "requires local binary access"
fi

if [[ "$TARGET_EXEC" != "-" ]]; then
  pgrep -f "$TARGET_EXEC -h $HOST -p $ECHO_PORT -l /tmp/tests-echo.log" 2>/dev/null | xargs -I {} kill -9 {} 2>/dev/null
fi

# Tests - Cron reload
# A cron job is installed at process start and re-installed only when its file
# changes. Regression: a _cron.wren created or edited while the server runs
# fell inside the 3s reload debounce window and was silently dropped, so the
# cron never took effect without a restart. Creating then editing the file must
# both re-install the cron immediately, right after startup (inside the window).
if [[ "$TARGET_EXEC" != "-" ]]; then
  cron_line=$LINENO
  _test_start_ms=$(now_ms)
  cron_root=$(mktemp -d)
  rm -f /tmp/tests-cron.log
  "$TARGET_EXEC" -h "$HOST" -p "$CRON_PORT" -l /tmp/tests-cron.log \
    "$cron_root" > /dev/null 2>&1 &
  disown
  wait_port "$HOST" "$CRON_PORT" 10
  cron_installs_before=$(grep -c "Installing cron" /tmp/tests-cron.log 2>/dev/null)
  cron_installs_before=${cron_installs_before:-0}
  # 75s per phase: install_cron() now also runs on every 60s cron tick (see
  # cron_thread() in main.c), as a safety net for when the dmon/FSEvents
  # watch itself never delivers the event -- confirmed happening on GitHub's
  # macOS runners, where a diagnostic run showed the *create* fsevent dropped
  # outright (not merely delayed) while the watch was still "starting up".
  # 75s covers that worst case (60s tick + margin) without weakening the
  # assertion; the common path (dmon fires promptly) still resolves in ~1s.
  printf 'Cron.every(1) { |d| System.print("CRON-MARKER") }\n' > "$cron_root/_cron.wren"
  cron_installs_create=0
  cron_deadline=$(( $(now_ms) + 75000 ))
  while [[ $cron_installs_create -le $cron_installs_before && $(now_ms) -lt $cron_deadline ]]; do
    cron_installs_create=$(grep -c "Installing cron" /tmp/tests-cron.log 2>/dev/null)
    cron_installs_create=${cron_installs_create:-0}
    [[ $cron_installs_create -le $cron_installs_before ]] && sleep 1
  done
  printf 'Cron.every(1) { |d| System.print("CRON-MARKER-EDIT") }\n' > "$cron_root/_cron.wren"
  cron_installs_edit=$cron_installs_create
  cron_deadline=$(( $(now_ms) + 75000 ))
  while [[ $cron_installs_edit -le $cron_installs_create && $(now_ms) -lt $cron_deadline ]]; do
    cron_installs_edit=$(grep -c "Installing cron" /tmp/tests-cron.log 2>/dev/null)
    cron_installs_edit=${cron_installs_edit:-0}
    [[ $cron_installs_edit -le $cron_installs_create ]] && sleep 1
  done
  pgrep -f "$TARGET_EXEC -h $HOST -p $CRON_PORT -l /tmp/tests-cron.log" \
    2>/dev/null | xargs -I {} kill -9 {} 2>/dev/null
  rm -rf "$cron_root"
  if [[ $cron_installs_create -gt $cron_installs_before \
        && $cron_installs_edit -gt $cron_installs_create ]]; then
    report_result "Cron reload on file create/edit" "$cron_line" 0
  else
    report_result "Cron reload on file create/edit" "$cron_line" 1 \
      "Creating/editing _cron.wren while the server runs did not re-install the cron. before:$cron_installs_before create:$cron_installs_create edit:$cron_installs_edit"
  fi
else
  skip_test "Cron reload on file create/edit" "requires local binary access"
fi

# Tests - bialet dev
# `bialet dev` must serve the current directory, enable BIALET_LIVE_RELOAD and
# BIALET_SHOW_ERRORS in the DB, inject the live-reload script into HTML, and
# bump the /_livereload version when a file in the app directory changes.
if [[ "$TARGET_EXEC" != "-" ]]; then
  dev_starts_line=$LINENO
  _test_start_ms=$(now_ms)
  dev_root="$(dirname "$0")/dev-app"
  dev_exec=$(realpath "$TARGET_EXEC")
  rm -f "$dev_root/_db.sqlite3"
  (cd "$dev_root" && "$dev_exec" dev -q -h "$HOST" -p "$DEV_PORT" -l /tmp/tests-dev.log) \
    > /dev/null 2>&1 &
  disown
  wait_http "http://$HOST:$DEV_PORT/" 200 10
  dev_code=$(curl -s -o /dev/null -w "%{http_code}" "http://$HOST:$DEV_PORT/")
  dev_body=$(curl -s "http://$HOST:$DEV_PORT/")
  dev_live=$(printf "%s" "$dev_body" | grep -c "_livereload")
  dev_flags=$(sqlite3 "$dev_root/_db.sqlite3" \
    "SELECT COUNT(*) FROM BIALET_CONFIG WHERE key IN ('BIALET_LIVE_RELOAD','BIALET_SHOW_ERRORS') AND val='1'" 2>/dev/null)
  dev_v1=$(curl -s "http://$HOST:$DEV_PORT/_livereload")
  sleep 1
  printf 'reload\n' > "$dev_root/reload_scratch"
  # The version is time(NULL), so it only bumps once the file-watch fires in a
  # new second. Poll for the bump instead of sleeping a fixed 2s.
  dev_v2=""
  reload_deadline=$(( $(now_ms) + 5000 ))
  while [[ "$dev_v2" == "$dev_v1" && $(now_ms) -lt $reload_deadline ]]; do
    dev_v2=$(curl -s "http://$HOST:$DEV_PORT/_livereload")
    [[ "$dev_v2" == "$dev_v1" ]] && sleep 1
  done
  rm -f "$dev_root/reload_scratch"
  pgrep -f "$dev_exec dev -q -h $HOST -p $DEV_PORT -l /tmp/tests-dev.log" 2>/dev/null | xargs -I {} kill -9 {} 2>/dev/null
  if [[ "$dev_code" == "200" && "$dev_body" == *"Dev App"* && "$dev_live" == "1" \
        && "$dev_flags" == "2" && "$dev_v1" != "$dev_v2" ]]; then
    report_result "bialet dev starts" "$dev_starts_line" 0
  else
    report_result "bialet dev starts" "$dev_starts_line" 1 \
      "Expected 200 + 'Dev App' + livereload script + 2 enabled flags + bumping version. Got code:$dev_code body:'$dev_body' livereload:$dev_live flags:$dev_flags v1:$dev_v1 v2:$dev_v2"
  fi
  rm -f "$dev_root/_db.sqlite3"
else
  skip_test "bialet dev starts" "requires local binary access"
fi

finish
print_summary >&2

exit $?
