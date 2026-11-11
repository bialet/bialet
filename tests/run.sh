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
MULTIPART_PORT="${args[7]:-7104}"

# The Windows binary is cross-compiled on Linux and exercised under Wine. Two
# tests rely on POSIX semantics Wine does not reproduce and fail identically on
# a clean HEAD (documented in AGENTS.md), so skip them rather than report
# environmental noise. A `.exe` target is the signal for that workflow.
IS_WINE=0
[[ "$TARGET_EXEC" == *.exe ]] && IS_WINE=1

source "$(dirname "$0")/util.sh"

# Start echo server used by the Http.* method tests (POST/PUT/DELETE targets)
if [[ "$TARGET_EXEC" != "-" ]]; then
  require_free_port "$HOST" "$ECHO_PORT" "echo server"
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

# Tests - bare -r initializes the framework tables
# A -r script has no migration and no request, so Config/Session used to fail
# with "no such table: BIALET_CONFIG". Db.init now runs before the script.
r_line=$LINENO
if [[ "$TARGET_EXEC" != "-" ]]; then
  r_dir="$(mktemp -d)"
  if [[ "$TARGET_EXEC" == */* ]]; then
    r_exec="$(cd "$(dirname "$TARGET_EXEC")" && pwd)/$(basename "$TARGET_EXEC")"
  else
    r_exec="$(command -v "$TARGET_EXEC")"
  fi
  r_out=$(cd "$r_dir" && "$r_exec" -r \
    $'Config.set("r_probe", "set")\nvar s = Session.new()\ns.set("r_session", "ok")\nreturn Config.get("r_probe") + "|" + s.get("r_session")' \
    2>/dev/null)
  r_status=$?
  rm -rf "$r_dir"
  if [[ "$r_status" -eq 0 && "$r_out" == "set|ok" ]]; then
    report_result "-r initializes framework tables" "$r_line" 0
  else
    report_result "-r initializes framework tables" "$r_line" 1 \
      "Expected 'set|ok', got '$r_out' (exit $r_status)"
  fi
else
  skip_test "-r initializes framework tables" "requires local binary access"
fi

# Tests - Module imports
# Regression: a relative import ("./x") inside a file that is itself only
# ever reached transitively (entry.wren -> ./sub/lib -> ./helper) must
# resolve against the importing file's own directory, not the entry file's.
if [[ "$IS_WINE" == 1 ]]; then
  skip_test "Nested relative import         " "Wine: documented environmental failure (see AGENTS.md)"
else
  run_test "Nested relative import         " "import-nested/entry" 200 "hi from helper"
fi

# Tests - Request & Response
run_test "Get the URL parameter       " "get?foo=bar"     200 "bar"
run_test "Get the post parameter      " "post" "foo=bar"  200 "bar"
run_test "Get the route parameter     " "route/baz/qux"   200 "bazqux"
# Regression: dynamic segment sharing a letter prefix with the route file name
# (e.g. file "reservar.wren", segment "reunion-30min" both start with "re") must
# not be truncated by the route-prefix stripping logic.
run_test "Route param w/ dir overlap   " "reservar/reunion-30min" 200 "reunion-30min"
# A single folder.wren file serves both the bare folder URL (Request.route(0)
# is null) and every deeper path (Request.route(n) reads the remaining
# segments) -- the classic list + detail resource pattern.
run_test "Route file base URL         " "notes"          200 "notes-list"
run_test "Route file with segment     " "notes/42"       200 "note:42"
# Regression: the query string used to leak into Request.route(), so /notes?page=2
# looked like the detail route "?page=2" and the handler answered 404.
run_test "Route ignores query string  " "notes?page=2"   200 "notes-list"
run_test "Redirection                 " "redirect"        302 ""
run_test "Forbid hidden file          " "_hidden"         403

# Framing: the server answers HTTP/1.1 and closes after every response, so the
# head must say `Connection: close` instead of leaving the client to infer
# persistence (HTTP/1.1 defaults to keep-alive).
_test_start_ms=$(now_ms)
framing_headers=$(curl -s -D - -o /dev/null --max-time 5 "http://$HOST:$PORT/")
if [[ "$framing_headers" == HTTP/1.1* && "$framing_headers" == *"Connection: close"* ]]; then
  report_result "HTTP/1.1 + Connection close    " "$LINENO" 0
else
  report_result "HTTP/1.1 + Connection close    " "$LINENO" 1 \
    "got: $(printf '%s' "$framing_headers" | head -1)"
fi

# RFC 8615: ".well-known" is the one exempt dot-segment. Everything below it
# is served -- static files and .wren routes alike -- while every other
# dot/underscore path keeps the 403 above. Fixtures live under
# tests/.well-known/.
run_test "Well-known static file      " ".well-known/openid-configuration" 200 "wk-static-oidc"
run_test "Well-known wren route       " ".well-known/oauth-authorization-server" 200 "wk-wren-oauth"
run_test "Well-known ACME challenge   " ".well-known/acme-challenge/token-abc123" 200 "wk-acme-token"
run_test "Well-known deep folder      " ".well-known/sub/folder/file" 200 "wk-deep"
run_test "Well-known JSON resource    " ".well-known/jwks.json" 200 "wk-jwks-json"

# The exemption is a path-prefix on the *first* segment only, and the match is
# exact and case-sensitive: a nested ".well-known" is still protected, and so
# is ".Well-Known". Protected paths return 403 whether or not they exist.
run_test "Well-known nested not exempt" "foo/.well-known/x" 403
run_test "Well-known case-sensitive   " ".Well-Known/x"   403
run_test "Forbid dotenv               " ".env"            403
run_test "Forbid git config           " ".git/config"     403
run_test "Forbid htaccess             " ".htaccess"       403
run_test "Forbid DS_Store             " ".DS_Store"       403
run_test "Forbid protected folder     " "_app/foo"        403

# Path traversal must not slip through the exemption. curl normalizes ".."
# and some percent-escapes, so send the raw path with --path-as-is.
well_known_traversal_line=$LINENO
_test_start_ms=$(now_ms)
wk_traversal_ok=1
wk_code=""
for wk_path in "/.well-known/../.env" "/%2e%2e/.env"; do
  wk_code=$(curl -s -o /dev/null -w "%{http_code}" --path-as-is \
    "http://$HOST:$PORT$wk_path")
  [[ "$wk_code" == "403" ]] || wk_traversal_ok=0
done
# A percent-encoded ".well-known" must not become an exempt prefix by
# accident: the server does not decode paths, so it resolves to nothing
# (404), never a 200 that would imply the exemption matched it.
wk_encoded=$(curl -s -o /dev/null -w "%{http_code}" --path-as-is \
  "http://$HOST:$PORT/%2ewell-known/openid-configuration")
[[ "$wk_encoded" != "200" ]] || wk_traversal_ok=0
if [[ "$wk_traversal_ok" == 1 ]]; then
  report_result "Well-known traversal blocked" "$well_known_traversal_line" 0
else
  report_result "Well-known traversal blocked" "$well_known_traversal_line" 1 \
    "Expected 403 for the traversal paths and non-200 for %2ewell-known. Got .env:$wk_code encoded:$wk_encoded"
fi

# Regression: a planted folder.wren -> ../_db.sqlite3 symlink must not bypass
# the private-file rule. The route-file search must not follow symlinks, and
# the resolved-path check re-validates the canonical target. The probe returns
# 404 (no route file found), never the leaked database bytes.
if [[ "$IS_WINE" == 1 ]]; then
  skip_test "folder.wren symlink no bypass" "Wine: documented environmental failure (see AGENTS.md)"
elif [[ "$FS_SHARED" == 1 ]]; then
  route_symlink_line=$LINENO
  _test_start_ms=$(now_ms)
  route_symlink_dir="$(dirname "$0")"
  route_symlink="$route_symlink_dir/sub.wren"
  rm -f "$route_symlink"
  route_symlink_code=""
  route_symlink_body=""
  if ln -s "../_db.sqlite3" "$route_symlink"; then
    route_symlink_code=$(curl -s -o /dev/null -w "%{http_code}" \
      "http://$HOST:$PORT/sub/does-not-exist")
    route_symlink_body=$(curl -s "http://$HOST:$PORT/sub/does-not-exist")
  fi
  rm -f "$route_symlink"
  if [[ "$route_symlink_code" == "404" && "$route_symlink_body" != "SQLite format 3"* ]]; then
    report_result "folder.wren symlink no bypass" "$route_symlink_line" 0
  else
    report_result "folder.wren symlink no bypass" "$route_symlink_line" 1 \
      "Expected 404, no DB bytes. Got code:$route_symlink_code body:'${route_symlink_body:0:40}'"
  fi
else
  skip_test "folder.wren symlink no bypass" "server filesystem is not shared with this script"
fi
# The ".wren" probe must only synthesize a name strictly below the root. The
# bare-root URI used to strip to full_root_dir exactly and probe
# "<root>.wren" - a sibling of the app directory - then open and interpret it;
# "//" probed "<root>/.wren", a dot-file a direct request forbids.
sibling_line=$LINENO
_test_start_ms=$(now_ms)
sibling_path="$(dirname "$0").wren"
dot_path="$(dirname "$0")/.wren"
printf 'return "SIBLING-OUTSIDE-ROOT"' > "$sibling_path"
sibling_home=$(curl -s --max-time 10 "http://$HOST:$PORT/")
sibling_query=$(curl -s --max-time 10 "http://$HOST:$PORT/?x")
rm -f "$sibling_path"
printf 'RAWDOTFILE' > "$dot_path"
dot_direct_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 10 "http://$HOST:$PORT/.wren")
dot_slash=$(curl -s --max-time 10 "http://$HOST:$PORT//")
rm -f "$dot_path"
if [[ "$sibling_home" != *"SIBLING-OUTSIDE-ROOT"* \
      && "$sibling_query" != *"SIBLING-OUTSIDE-ROOT"* \
      && "$dot_direct_code" == "403" \
      && "$dot_slash" != *"RAWDOTFILE"* ]]; then
  report_result "Root probe stays in root      " "$sibling_line" 0
else
  report_result "Root probe stays in root      " "$sibling_line" 1 \
    "Expected / and /?x to skip <root>.wren and // to skip <root>/.wren. Got home:'${sibling_home:0:80}' query:'${sibling_query:0:80}' dot:$dot_direct_code slash:'${dot_slash:0:40}'"
fi

# Regression: the execute-vs-serve decision compared the requested suffix with
# a case-sensitive strcmp, but the file probe resolves case-insensitively on
# APFS/NTFS. "/get.WREN" therefore opened get.wren and returned its source text
# instead of running it. Whatever the filesystem does, the source must never
# come back; on a case-insensitive one the route runs as usual.
wren_case_line=$LINENO
_test_start_ms=$(now_ms)
wren_case_code=$(curl -s -o /dev/null -w "%{http_code}" \
  --max-time 10 "http://$HOST:$PORT/get.WREN?foo=bar")
wren_case_body=$(curl -s --max-time 10 "http://$HOST:$PORT/get.WREN?foo=bar")
wren_case_mixed=$(curl -s --max-time 10 "http://$HOST:$PORT/Get.Wren?foo=bar")
wren_case_ok=1
[[ "$wren_case_body" != *"Request.get"* ]] || wren_case_ok=0
[[ "$wren_case_mixed" != *"Request.get"* ]] || wren_case_ok=0
# A 200 may only come from the executed route, never from the file bytes.
if [[ "$wren_case_code" == "200" && "$wren_case_body" != "bar" ]]; then
  wren_case_ok=0
fi
if [[ "$wren_case_ok" == 1 ]]; then
  report_result "Wren suffix case-insensitive " "$wren_case_line" 0
else
  report_result "Wren suffix case-insensitive " "$wren_case_line" 1 \
    "Expected /get.WREN to execute or 404, never the module source. Got code:$wren_case_code body:'${wren_case_body:0:60}'"
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

# Regression for the quadratic inline-Markdown scan: every '[' without a
# complete '](...)' used to repeat three full-line strchr() calls, and
# Request.init joins the body into one line, so a bracket body cost O(n^2) CPU
# on the single worker. The server runs with the body cap raised so the
# difference is unambiguous: this body is ~9 s when quadratic and a few
# hundredths of a second once the failed delimiter lookup is latched.
md_scan_line=$LINENO
_test_start_ms=$(now_ms)
md_scan_ok=1
md_scan_result="skipped"
if [[ "$TARGET_EXEC" != "-" ]]; then
  md_scan_db="$(mktemp /tmp/bialet-mdscan.XXXXXX)"
  md_scan_body="$(mktemp /tmp/bialet-mdscan-body.XXXXXX)"
  head -c 786432 /dev/zero | tr '\0' '[' > "$md_scan_body"
  require_free_port "$HOST" "$MULTIPART_PORT" "markdown scan server"
  "$TARGET_EXEC" -h "$HOST" -p "$MULTIPART_PORT" -l /tmp/tests-mdscan.log \
    -m 512 -M 1024 -b 768 -d "$md_scan_db" "$(dirname "$0")" \
    > /dev/null 2>&1 &
  disown
  wait_port "$HOST" "$MULTIPART_PORT" 10
  md_scan_time=$(curl -s -o /dev/null -w "%{time_total}" --max-time 30 \
    -H "Content-Type: text/plain" --data-binary "@$md_scan_body" \
    "http://$HOST:$MULTIPART_PORT/md-body")
  md_scan_head=$(curl -s --max-time 30 -H "Content-Type: text/plain" \
    --data-binary "@$md_scan_body" "http://$HOST:$MULTIPART_PORT/md-body" |
    head -c 7)
  # Matched links, images and unmatched text must still render exactly as before.
  md_scan_link=$(curl -s --max-time 10 -H "Content-Type: text/plain" \
    --data-binary '[a](b)' "http://$HOST:$MULTIPART_PORT/md-body")
  md_scan_img=$(curl -s --max-time 10 -H "Content-Type: text/plain" \
    --data-binary '![i](j)' "http://$HOST:$MULTIPART_PORT/md-body")
  md_scan_text=$(curl -s --max-time 10 -H "Content-Type: text/plain" \
    --data-binary '[x] ![y]' "http://$HOST:$MULTIPART_PORT/md-body")
  pgrep -f "$TARGET_EXEC -h $HOST -p $MULTIPART_PORT -l /tmp/tests-mdscan.log" \
    2>/dev/null | xargs -I {} kill -9 {} 2>/dev/null
  rm -f "$md_scan_body" "$md_scan_db" "$md_scan_db"-*
  md_scan_result="time:${md_scan_time} head:${md_scan_head} link:${md_scan_link} img:${md_scan_img} text:${md_scan_text}"
  if [[ "$md_scan_head" != "<p>[[[[" ]] ||
     [[ "$md_scan_link" != '<p><a href="b">a</a></p>' ]] ||
     [[ "$md_scan_img" != '<p><img alt="i" src="j"></p>' ]] ||
     [[ "$md_scan_text" != '<p>[x] ![y]</p>' ]] ||
     ! awk -v t="$md_scan_time" 'BEGIN { exit !(t < 2.0) }'; then
    md_scan_ok=0
  fi
else
  md_scan_ok=2
fi
if [[ "$md_scan_ok" == 2 ]]; then
  skip_test "Markdown unmatched brackets  " "requires local binary access"
elif [[ "$md_scan_ok" == 1 ]]; then
  report_result "Markdown unmatched brackets  " "$md_scan_line" 0
else
  report_result "Markdown unmatched brackets  " "$md_scan_line" 1 \
    "Expected '<p>[[[[' in under 2s for $md_scan_body, got $md_scan_result"
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

# Per-file upload cap (-u): the effective limit with default flags is ~120 KB
# (128 KB body cap minus ~8 KB of multipart framing). A 100 KB file must be
# accepted; a 124 KB file fits inside the 128 KB body but must be rejected as
# over the per-file cap, so upload.wren answers 400 "No file was uploaded".
upload_ok_line=$LINENO
_test_start_ms=$(now_ms)
upload_file="$(mktemp /tmp/bialet-upload-ok.XXXXXX)"
head -c 102400 /dev/zero > "$upload_file" 2>/dev/null
upload_ok_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 20 \
  -F "form_file_name=@$upload_file" "http://$HOST:$PORT/upload")
upload_ok_body=$(curl -s --max-time 20 \
  -F "form_file_name=@$upload_file" "http://$HOST:$PORT/upload")
rm -f "$upload_file"
if [[ "$upload_ok_code" == "200" && "$upload_ok_body" == *"102400"* ]]; then
  report_result "Upload within -u limit        " "$upload_ok_line" 0
else
  report_result "Upload within -u limit        " "$upload_ok_line" 1 \
    "Expected 200 with size 102400. Got status:$upload_ok_code body:'$upload_ok_body'"
fi

upload_over_line=$LINENO
_test_start_ms=$(now_ms)
upload_file="$(mktemp /tmp/bialet-upload-over.XXXXXX)"
head -c 126976 /dev/zero > "$upload_file" 2>/dev/null  # 124 KB, over the ~120 KB cap
upload_over_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 20 \
  -F "form_file_name=@$upload_file" "http://$HOST:$PORT/upload")
upload_over_body=$(curl -s --max-time 20 \
  -F "form_file_name=@$upload_file" "http://$HOST:$PORT/upload")
rm -f "$upload_file"
if [[ "$upload_over_code" == "400" && "$upload_over_body" == *"No file was uploaded"* ]]; then
  report_result "Upload over -u limit rejected " "$upload_over_line" 0
else
  report_result "Upload over -u limit rejected " "$upload_over_line" 1 \
    "Expected 400 over the cap. Got status:$upload_over_code body:'$upload_over_body'"
fi

# Header-block agreement: a whitespace-only line must end the header block for
# both the C multipart detector and the app-visible map, and a field name with
# whitespace before the colon must be invisible to both. Otherwise a request the
# app classifies as text/plain still stores multipart files, and one it
# classifies as multipart delivers no parts and the raw framing as the body.
if [[ "$TARGET_EXEC" != "-" ]] && command -v python3 >/dev/null 2>&1; then
  mpdetect_line=$LINENO
  _test_start_ms=$(now_ms)
  mpdetect_out=$(python3 - "$HOST" "$PORT" <<'PY'
import socket, sys

host, port = sys.argv[1], int(sys.argv[2])
body = (b'--zz\r\nContent-Disposition: form-data; name="note"\r\n\r\n'
        b'injected\r\n--zz\r\nContent-Disposition: form-data; name="doc"; '
        b'filename="e.txt"\r\nContent-Type: text/plain\r\n\r\nPAY\r\n--zz--\r\n')

def send(extra):
    head = (b'POST /multipart-detect HTTP/1.1\r\nHost: x\r\nConnection: close\r\n'
            b'Content-Length: ' + str(len(body)).encode() + b'\r\n' + extra + b'\r\n')
    s = socket.create_connection((host, port), 10)
    s.settimeout(20)
    s.sendall(head + body)
    data = b''
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
    except OSError:
        pass
    s.close()
    return data.split(b'\r\n\r\n', 1)[1].decode('utf-8', 'replace').strip()

ws = send(b'Content-Type: text/plain\r\n \r\n'
          b'Content-Type: multipart/form-data; boundary=zz\r\n')
colon = send(b'Content-Type : multipart/form-data; boundary=zz\r\n')
control = send(b'Content-Type: multipart/form-data; boundary=zz\r\n')

def status(extra):
    head = (b'POST /multipart-detect HTTP/1.1\r\nHost: x\r\nConnection: close\r\n'
            b'Content-Length: ' + str(len(body)).encode() + b'\r\n' + extra + b'\r\n')
    s = socket.create_connection((host, port), 10)
    s.settimeout(20)
    s.sendall(head + body)
    data = b''
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
    except OSError:
        pass
    s.close()
    return int(data.split(b' ')[1]) if data.startswith(b'HTTP/') else 0

# A bare CR or a bare LF inside the header block splits it differently in the
# C detector and the Wren map, so the block is refused outright.
bare_cr = status(b'Content-Type: application/json\r\n\r\r\n'
                 b'Content-Type: multipart/form-data; boundary=zz\r\n')
bare_lf = status(b'Content-Type: multipart/form-data; boundary=zz\r\n'
                 b'Content-Type\n\r\n')
print('ws:%s|colon:%s|control:%s|barecr:%d|barelf:%d' %
      (ws, colon, control, bare_cr, bare_lf))
PY
)
  if [[ "$mpdetect_out" == "ws:ct:text/plain|note:none|file:none|colon:ct:none|note:none|file:none|control:ct:multipart/form-data; boundary=zz|note:injected|file:e.txt:3|barecr:400|barelf:400" ]]; then
    report_result "Multipart header agreement    " "$mpdetect_line" 0
  else
    report_result "Multipart header agreement    " "$mpdetect_line" 1 \
      "Expected both parsers to agree on the header block. Got $mpdetect_out"
  fi
else
  skip_test "Multipart header agreement    " "requires local binary access and python3"
fi

# App-supplied log text: System.log() writes the operator log, stdout and
# BIALET_LOGS, so a request value logged by the app (the documented shape) must
# be escaped exactly like the framework's own wire-derived log values.
if [[ "$TARGET_EXEC" != "-" ]] && command -v python3 >/dev/null 2>&1; then
  applog_line=$LINENO
  _test_start_ms=$(now_ms)
  applog_out=$(python3 - "$HOST" "$PORT" /tmp/tests.log <<'PY'
import http.client, sys, time

host, port, logpath = sys.argv[1], int(sys.argv[2]), sys.argv[3]
try:
    offset = len(open(logpath, 'rb').read())
except OSError:
    offset = 0

conn = http.client.HTTPConnection(host, port, timeout=20)
body = 'v=abc%0aFORGED%1b%5b31mEND'
conn.request('POST', '/log-escape', body,
             {'Content-Type': 'application/x-www-form-urlencoded'})
resp = conn.getresponse()
resp.read()
conn.close()
time.sleep(0.3)
try:
    log = open(logpath, 'rb').read()[offset:]
except OSError:
    log = b''
print("status:%d|escaped:%s|rawEsc:%s|forged:%s" % (
    resp.status,
    b'Log probe abc\\x0aFORGED\\x1b[31mEND' in log,
    b'\x1b' in log,
    b'\nFORGED' in log))
PY
)
  if [[ "$applog_out" == "status:200|escaped:True|rawEsc:False|forged:False" ]]; then
    report_result "App log text escaped         " "$applog_line" 0
  else
    report_result "App log text escaped         " "$applog_line" 1 \
      "Expected one escaped Log record and no raw ESC. Got $applog_out"
  fi
else
  skip_test "App log text escaped         " "requires local binary access and python3"
fi

# A rejected upload logs the wire-supplied filename: it must be escaped, so a
# control byte cannot inject terminal sequences or split the record. The
# filename is sent raw over a socket (no form encoder) with an over-limit part.
if [[ "$TARGET_EXEC" != "-" ]] && command -v python3 >/dev/null 2>&1; then
  loginj_line=$LINENO
  _test_start_ms=$(now_ms)
  loginj_out=$(python3 - "$HOST" "$PORT" /tmp/tests.log <<'PY'
import socket, sys, time

host, port, logpath = sys.argv[1], int(sys.argv[2]), sys.argv[3]
name = b'ev\x1b[2Jil.txt'
payload = b'x' * 126976

# The log is opened in append mode, so only bytes written from here on count:
# a stale record from an earlier run must not decide this check.
try:
    offset = len(open(logpath, 'rb').read())
except OSError:
    offset = 0

boundary = b'BOUND'
body = (b'--' + boundary +
        b'\r\nContent-Disposition: form-data; name="form_file_name"; filename="' +
        name + b'"\r\n\r\n' + payload + b'\r\n--' + boundary + b'--\r\n')
req = (b'POST /upload HTTP/1.1\r\nHost: x\r\n'
       b'Content-Type: multipart/form-data; boundary=' + boundary +
       b'\r\nContent-Length: ' + str(len(body)).encode() + b'\r\n\r\n' + body)
s = socket.create_connection((host, port), 10)
s.settimeout(20)
s.sendall(req)
resp = b''
try:
    while True:
        chunk = s.recv(4096)
        if not chunk:
            break
        resp += chunk
except OSError:
    pass
s.close()
time.sleep(0.3)
try:
    log = open(logpath, 'rb').read()
except OSError:
    log = b''
print("status:%d|escaped:%s|raw:%s" % (
    int(resp.split(b' ')[1]) if resp.startswith(b'HTTP/') and len(resp.split(b' ')) > 1 else 0,
    b'ev\\x1b[2Jil.txt' in log,
    b'\x1b[2Jil.txt' in log))
PY
)
  if [[ "$loginj_out" == "status:400|escaped:True|raw:False" ]]; then
    report_result "Upload filename log escaped " "$loginj_line" 0
  else
    report_result "Upload filename log escaped " "$loginj_line" 1 \
      "Expected 400 with the filename hex-escaped in the log and no raw ESC. Got $loginj_out"
  fi
else
  skip_test "Upload filename log escaped " "requires local binary access and python3"
fi

# Multipart text fields: Request.post must read the text fields of a
# multipart form next to Request.file, in any order around the file.
multipart_line=$LINENO
_test_start_ms=$(now_ms)
mp_file="$(mktemp /tmp/bialet-multipart.XXXXXX)"
printf 'hi' > "$mp_file"
mp_both=$(curl -s --max-time 20 -F "a=hola" -F "excel=@$mp_file;filename=x.csv" \
  -F "b=after" "http://$HOST:$PORT/multipart")
mp_text=$(curl -s --max-time 20 -F "a=hola" "http://$HOST:$PORT/multipart")
mp_rich=$(curl -s --max-time 20 -F $'note=línea1\nlínea2 a=b&c' \
  "http://$HOST:$PORT/multipart")
mp_urlencoded=$(curl -s --max-time 20 -d "a=url" "http://$HOST:$PORT/multipart")
rm -f "$mp_file"
if [[ "$mp_both" == "a=hola|b=after|"*"|file=x.csv:2" \
      && "$mp_both" == *"excel=null"* \
      && "$mp_text" == "a=hola|"*"|file=none" \
      && "$mp_rich" == *$'note=línea1\nlínea2 a=b&c|'* \
      && "$mp_urlencoded" == "a=url|"* ]]; then
  report_result "Multipart text fields          " "$multipart_line" 0
else
  report_result "Multipart text fields          " "$multipart_line" 1 \
    "Got both:'$mp_both' text:'$mp_text' rich:'$mp_rich' urlencoded:'$mp_urlencoded'"
fi

# CSRF: Session.csrfOk must accept the token sent as a multipart field.
mp_csrf_line=$LINENO
_test_start_ms=$(now_ms)
mp_cookie="$(mktemp /tmp/bialet-mp-csrf.XXXXXX)"
mp_token=$(curl -s -c "$mp_cookie" "http://$HOST:$PORT/csrf" \
  | grep -o 'value="[^"]*"' | sed 's/value="//; s/"//')
mp_csrf_ok=$(curl -s -b "$mp_cookie" -F "_bialet_csrf=$mp_token" -F "x=1" \
  "http://$HOST:$PORT/csrf")
mp_csrf_bad=$(curl -s -b "$mp_cookie" -F "_bialet_csrf=wrong" "http://$HOST:$PORT/csrf")
rm -f "$mp_cookie"
if [[ -n "$mp_token" && "$mp_csrf_ok" == "ok" && "$mp_csrf_bad" == "fail" ]]; then
  report_result "Multipart CSRF token           " "$mp_csrf_line" 0
else
  report_result "Multipart CSRF token           " "$mp_csrf_line" 1 \
    "Expected ok/fail. Got token:'$mp_token' ok:'$mp_csrf_ok' bad:'$mp_csrf_bad'"
fi

# Reading an upload must not persist it: Request.file() used to call .save, so
# a handler that inspected a file and rejected it still moved the row out of the
# temporary pool and out of the one-day purge, letting a client grow the shared
# database without bound.
mp_persist_line=$LINENO
_test_start_ms=$(now_ms)
mp_persist_file="$(mktemp /tmp/bialet-persist.XXXXXX)"
printf 'hello' > "$mp_persist_file"
mp_persist=$(curl -s --max-time 20 -F "form_file_name=@$mp_persist_file" \
  "http://$HOST:$PORT/upload-persist")
rm -f "$mp_persist_file"
if [[ "$mp_persist" == "isTemp:true|rowAfterRead:1|rowAfterSave:0" ]]; then
  report_result "Reading upload keeps it temp  " "$mp_persist_line" 0
else
  report_result "Reading upload keeps it temp  " "$mp_persist_line" 1 \
    "Expected the row to stay temporary until .save. Got '$mp_persist'"
fi

# A large upload (5 MB) next to a text field, on a server with the limits
# raised: the field must survive and the upload must not fail on memory.
if [[ "$TARGET_EXEC" != "-" ]]; then
  mp_big_line=$LINENO
  _test_start_ms=$(now_ms)
  mp_db="$(mktemp /tmp/bialet-mp-big.XXXXXX)"
  mp_big="$(mktemp /tmp/bialet-mp-big-file.XXXXXX)"
  head -c 5242880 /dev/zero > "$mp_big" 2>/dev/null
  require_free_port "$HOST" "$MULTIPART_PORT" "multipart server"
  "$TARGET_EXEC" -h "$HOST" -p "$MULTIPART_PORT" -l /tmp/tests-multipart.log \
    -m 5300 -M 10600 -b 10500 -u 10MB -d "$mp_db" "$(dirname "$0")" \
    > /dev/null 2>&1 &
  disown
  wait_port "$HOST" "$MULTIPART_PORT" 10
  mp_big_body=$(curl -s --max-time 60 -F "a=before" -F "excel=@$mp_big" \
    -F "b=after" "http://$HOST:$MULTIPART_PORT/multipart")
  # Many parts with no blank line after their headers: each one used to rescan
  # the rest of the body, so this took minutes. It must be linear.
  mp_many_code="skipped"
  if command -v python3 >/dev/null 2>&1; then
    mp_many="$(mktemp /tmp/bialet-mp-many.XXXXXX)"
    python3 -c "
b = '--BND\r\nContent-Disposition: form-data; name=\"a\"\r\n'
open('$mp_many', 'w').write(b * 100000 + '--BND--\r\n')"
    mp_many_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 20 \
      -H "Content-Type: multipart/form-data; boundary=BND" \
      --data-binary "@$mp_many" "http://$HOST:$MULTIPART_PORT/multipart")
    rm -f "$mp_many"
  fi
  mp_big_alive=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 \
    "http://$HOST:$MULTIPART_PORT/multipart")
  pgrep -f "$TARGET_EXEC -h $HOST -p $MULTIPART_PORT -l /tmp/tests-multipart.log" \
    2>/dev/null | xargs -I {} kill -9 {} 2>/dev/null
  rm -f "$mp_big" "$mp_db" "$mp_db"-*
  if [[ "$mp_big_body" == "a=before|b=after|"*"|file="*":5242880" \
        && "$mp_many_code" != "000" && "$mp_big_alive" == "200" ]]; then
    report_result "Multipart 5 MB + many parts    " "$mp_big_line" 0
  else
    report_result "Multipart 5 MB + many parts    " "$mp_big_line" 1 \
      "Expected both fields and a 5242880-byte file. Got body:'${mp_big_body:0:200}' many-parts:$mp_many_code alive:$mp_big_alive"
  fi
else
  skip_test "Multipart 5 MB + many parts    " "requires local binary access"
fi

# Aggregate temp-upload ceiling: a file part with a filename and no data is a
# legal row of size 0, so a byte-only ceiling never engaged and 100-part
# requests grew BIALET_FILES without bound. The retained set must be bounded in
# rows too and the aggregate must come from the isTemp index rather than a full
# scan. A full pool must evict the oldest rows instead of refusing every later
# upload: otherwise 41 anonymous requests deny uploads app-wide for a day.
if [[ "$TARGET_EXEC" != "-" ]]; then
  cap_line=$LINENO
  _test_start_ms=$(now_ms)
  cap_db="$(mktemp /tmp/bialet-cap.XXXXXX)"
  cap_body="$(mktemp /tmp/bialet-cap-body.XXXXXX)"
  require_free_port "$HOST" "$MULTIPART_PORT" "upload ceiling server"
  "$TARGET_EXEC" -h "$HOST" -p "$MULTIPART_PORT" -l /tmp/tests-cap.log \
    -d "$cap_db" "$(dirname "$0")" > /dev/null 2>&1 &
  disown
  wait_port "$HOST" "$MULTIPART_PORT" 10
  cap_first="skipped"
  cap_last="skipped"
  cap_usage="skipped"
  cap_legit="skipped"
  if command -v python3 >/dev/null 2>&1; then
    python3 -c "
part = '--zz\r\nContent-Disposition: form-data; name=\"g\"; filename=\"z\"\r\n\r\n\r\n'
open('$cap_body', 'w').write(part * 100 + '--zz--\r\n')"
    cap_first=$(curl -s -o /dev/null -w "%{http_code}" --max-time 20 \
      -H "Content-Type: multipart/form-data; boundary=zz" \
      --data-binary "@$cap_body" "http://$HOST:$MULTIPART_PORT/multipart")
    for _ in $(seq 1 44); do
      cap_last=$(curl -s -o /dev/null -w "%{http_code}" --max-time 20 \
        -H "Content-Type: multipart/form-data; boundary=zz" \
        --data-binary "@$cap_body" "http://$HOST:$MULTIPART_PORT/multipart")
    done
    cap_usage=$(curl -s --max-time 5 "http://$HOST:$MULTIPART_PORT/upload-usage")
    cap_legit=$(curl -s -o /dev/null -w "%{http_code}" --max-time 20 \
      -F "a=1" -F "excel=@$cap_body" "http://$HOST:$MULTIPART_PORT/multipart")
    # A large part must still be admitted while retained rows are evictable:
    # the eviction measures the bytes it frees instead of estimating them from
    # the pool mean, which used to refuse this while megabytes were reclaimable.
    cap_big_file="$(mktemp /tmp/bialet-cap-big.XXXXXX)"
    head -c 120000 /dev/zero > "$cap_big_file" 2>/dev/null
    cap_big=$(curl -s -o /dev/null -w "%{http_code}" --max-time 20 \
      -F "form_file_name=@$cap_big_file" "http://$HOST:$MULTIPART_PORT/upload")
    rm -f "$cap_big_file"
  fi
  pgrep -f "$TARGET_EXEC -h $HOST -p $MULTIPART_PORT -l /tmp/tests-cap.log" \
    2>/dev/null | xargs -I {} kill -9 {} 2>/dev/null
  rm -f "$cap_body" "$cap_db" "$cap_db"-*
  if [[ "$cap_first" == "200" && "$cap_last" == "200" \
        && "$cap_usage" == "rows:4096|index:true|scan:" \
        && "$cap_legit" == "200" && "$cap_big" == "200" ]]; then
    report_result "Upload ceiling bounds rows     " "$cap_line" 0
  else
    report_result "Upload ceiling bounds rows     " "$cap_line" 1 \
      "Expected every fill and the later upload to answer 200 with the pool bounded at 4096 rows from the index. Got first:$cap_first last:$cap_last usage:'$cap_usage' legit:$cap_legit big:$cap_big"
  fi
else
  skip_test "Upload ceiling bounds rows     " "requires local binary access"
fi

# A complete header block with a declared Content-Length, and no body byte
# behind it, must not park the only worker for the full 30 s body budget; the
# 413 drain path must not either. One stalled socket, then a probe from a second
# connection: the probe has to be answered within the 5 s socket timeout, not
# the 30 s body deadline.
if [[ "$TARGET_EXEC" != "-" ]] && command -v python3 >/dev/null 2>&1; then
  stall_line=$LINENO
  _test_start_ms=$(now_ms)
  stall_out=$(python3 - "$HOST" "$PORT" <<'PY'
import socket, sys, time

host, port = sys.argv[1], int(sys.argv[2])

def probe():
    start = time.time()
    s = socket.create_connection((host, port), 10)
    s.settimeout(60)
    s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\n\r\n")
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = s.recv(4096)
        if not chunk:
            break
        data += chunk
    s.close()
    return time.time() - start

def stall(payload):
    s = socket.create_connection((host, port), 10)
    s.sendall(payload)
    return s

baseline = probe()
# Zero body bytes: the peer never sends anything after the header block.
stalled = stall(b"POST /echo HTTP/1.1\r\nHost: x\r\nContent-Length: 1\r\n\r\n")
time.sleep(0.3)
body = probe()
stalled.close()
# One body byte in a second segment: this used to buy the full 30 s budget.
started = stall(b"POST /echo HTTP/1.1\r\nHost: x\r\nContent-Length: 2\r\n\r\n")
time.sleep(0.5)
started.sendall(b"A")
started_body = probe()
started.close()
oversized = stall(b"POST /echo HTTP/1.1\r\nHost: x\r\nContent-Length: 999999\r\n\r\n")
time.sleep(0.3)
drain = probe()
oversized.close()
print("baseline=%.2f body=%.2f started=%.2f drain=%.2f" %
      (baseline, body, started_body, drain))
PY
)
  stall_body=$(printf '%s' "$stall_out" | sed -n 's/.*body=\([0-9.]*\).*/\1/p')
  stall_drain=$(printf '%s' "$stall_out" | sed -n 's/.*drain=\([0-9.]*\).*/\1/p')
  stall_started=$(printf '%s' "$stall_out" | sed -n 's/.*started=\([0-9.]*\).*/\1/p')
  if awk -v b="$stall_body" -v s="$stall_started" -v d="$stall_drain" \
      'BEGIN { exit !(b != "" && s != "" && d != "" && b < 12 && s < 12 && d < 12) }'; then
    report_result "Stalled body bounded          " "$stall_line" 0
  else
    report_result "Stalled body bounded          " "$stall_line" 1 \
      "Expected every probe under 12s (5s socket timeout, not the 30s body deadline). Got $stall_out"
  fi
else
  skip_test "Stalled body bounded          " "requires local binary access and python3"
fi

run_test "Response page escapes title " "response-page" 200 "Page&lt;title&gt;"
run_test "Response page escapes msg   " "response-page" 200 "Hello &amp; welcome"
run_test "Response out buffer         " "response-out"  200 "out:[first"
run_test "Response out HTML literal   " "response-out-html" 200 "<p>hi</p>"
run_test "Response headers getter     " "response-headers" 200 "custom:true|cookie:true"
run_test "Response forbidden          " "response-errors?forbidden=1" 403
run_test "Response notFound custom    " "response-errors?notfound=1" 404 "custom-404-page"
run_test "Cookie delete and defaults  " "cookie-delete" 200 "present:empty|fallback:fallback-value|missing:null"
run_test "Http jar is host-only        " "http-cookie-host-only" 200 "cross:[]|own:[a1=evil]|hosted:[]|parent:[]|sub:[]"
run_test "Session meta                " "session-meta" 200 "sidLen:40|default:BIALETSESSID|renamed:MYTESTCOOKIE|got:meta_value"
run_test "Session rotate rekeys writes" "session-rotate" 200 "planted:none|fresh:alice"
run_test "Session rows stay bounded " "session-sweep" 200 "live:bounded"
run_test "Directory index resolution  " "blog"           200 "route-index"
run_test "Directory index trailing /  " "blog/"          200 "route-index"
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
run_test "Query toNum method          " "query-tonum"     200 "empty:0|0|missing:0|50"
run_test "Query save method           " "save"            200 "hello updated 2024-09-13 10:30:00"
run_test "Query toBool method         " "db-tobool"       200 "true,false,true,true,true"
run_test "Query non-primitive params  " "query-param-stringify" 200 "node-name|tail|node-name"
run_test "Query to(Class) mapping     " "db-to-class"     200 "alpha:10,beta:20"
run_test "Query RETURNING clause      " "db-returning"    200 "5,5,5"
run_test "Db save delete migrate      " "db-more"         200 "inserted:"
run_test "Db delete validates table     " "db-delete-guard" 200 "literal:1|aborted:true|intact:1|other:2|refused:false|session:1"
run_test "Query binds reject NUL bytes  " "nul-bind" 200 "paramAborted:true|textAborted:true|bindAborted:true|control:admin"
run_test "For-in rejects non-instances   " "iterate-guard" 200 "bool:true|num:true|null:true|missing:true|rowValues:true|rowKeys:true|fromString:true|fetchFromString:true|saveBool:true|saveMissing:true|control:9"
run_test "NULL column maps to null     " "db-null"         200 "literal:true|bound:true"
run_test "UTF-8 range slices stay intact" "utf8-range" "v=café" 200 "lit:true|litHex:63,61,66,c3,a9|emoji:true|emojiHex:61,f0,9f,98,80,62|reverseHex:c3,a9,66,61,63|reverse:true|tailHex:a9|midHex:f0,9f,98,80|split:true|splitHex:64,c3,b3,73|trim:true|trimHex:73,65,c3,b1,6f,72|replace:true|replaceHex:63,61,66,c3,a9,20,73,69,6e,20,6c,65,63,68,65|posted:true|postedHex:63,61,66,c3,a9"

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
  run_test "Http cookie jar domain scope" "http-options?which=cookie-domain&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "set|session=abc123|no-cookie"
  run_test "Http cookie jar secure flag " "http-options?which=cookie-secure&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "set|no-cookie"
  run_test "Http cookie jar expire      " "http-options?which=cookie-expire&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "set|no-cookie"
  run_test "Http cookie jar override    " "http-options?which=cookie-override&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "set|set|session=second"
  run_test "Http shortcuts never crash   " "http-error-handling?target=http://$HOST:$ECHO_PORT" 200 \
    "down:true|downError:true|downHasMessage:true|missing:true|missingStatus:404|missingError:false"
  run_test "Http origin parse matches transport" "url-scope" 200 "weird:attacker.example:80|plain:127.0.0.1:18081|query:a.example:80|case:example.com:80|crossPort:false|sameHost:true|httpsDefault:443|userinfoAborted:true|fragmentStripped:http://a.example:80/x|monotonic:true"
  run_test "Http follows same-host redirect" "http-redirect?which=follow&target=http://$HOST:$ECHO_PORT" 200 "redirected"
  run_test "Http redirect cookie scoped   " "http-redirect?which=cookie&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "set|no-cookie|no-cookie|session=abc123"
  # The Win32 transport's URL parser rejects "host:port@host" (the port field
  # does not fit), so the request fails closed there and no cookie is ever
  # keyed. The invariant still holds; only the positive control needs libcurl.
  if [[ "$IS_WINE" == 1 ]]; then
    skip_test "Http userinfo key is real host" "Wine: Win32 URL parser rejects userinfo with a port (fails closed)"
  else
    run_test "Http userinfo key is real host" "http-redirect?which=userinfo&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "set|no-cookie|session=abc123"
  fi
  run_test "Http credential strip by name " "http-redirect?which=case&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "no-auth|no-auth|no-auth"
  run_test "Http no credential when unset " "http-redirect?which=noauth&target=http://$HOST:$ECHO_PORT" 200 "no-auth"
  run_test "Http reuse keeps per-call auth" "http-redirect?which=reuse&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "here:Basic cmV1c2U6cHc=|other:no-auth|form:a=b|formother:no-k=no-v"
  run_test "Http token not written to map" "http-redirect?which=shared&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "Bearer bearer-shared|no-auth|intact"
  run_test "Http redirect cookie control  " "http-redirect?which=control&target=http://$HOST:$ECHO_PORT" 200 "set|session=abc123"
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
run_test "Date comparison operators   " "date-compare"    200 "true,true,true,true,true,true,false,true,false,true"
run_test "Date parse from string      " "date-parse"      200 "2024-12-25 14:30:45,UTC"
run_test "Date constructors and get   " "date-more"       200 "dow:5|doy:366|woy:37|diff:86400|fmt:2024-09-13"
run_test "Date from unix timestamp   " "date-unix"       200 "round:2024-09-13 10:30:45|eq:true|zero:true"

# Tests - Util functions
run_test "Util functions              " "util"            200 "true"
run_test "Util encoding and helpers   " "util-more"       200 "hex:255|hexLower:26|toHex:FF|lpad:007|rev:cba"
run_test "SHA-256 and base64 helpers  " "sha256"         200 "all-passed"

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
run_test "Bare percent is literal     " "percent"         200 "100% 50%|world|2%|%("
run_test "Map get with default        " "map-get"         200 "a:1|missing:null|default:none|present:1|contains:true"
run_test "List reverse                " "list-reverse"    200 "rev:3,2,1|call:5,4|empty:0|orig:1,2,3"
run_test "User class null equality    " "user-eq-null"    200 "false,true,false,false,true,true"
run_test "toString accepts parentheses" "tostring"       200 "abc|42|true||custom|true"
run_test "Native string primitives  " "string-native"   200 "split:a||b||empty:1:true|one:a|trim:[x]|safe:&lt;&quot;&amp;&apos;&gt;|eq:truefalsefalse"
run_test "More extensions             " "extensions-more" 200 "toNum:42,null|map:Alice|list:2,A,B|take:1,2"
run_test "Num.fromString null         " "num-fromstring"  200 "null:null|bad:null|good:42"

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
  require_free_port "$HOST" "$SHOW_ERRORS_PORT" "show-errors server"
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
#
# BIALET_SKIP_CRON_RELOAD_TEST=1 skips this test: GitHub's hosted macOS
# runners have been observed to drop the underlying FSEvents notification
# outright (not just delay it), which this app-level test can't distinguish
# from a real regression. Not reproducible on a local macOS install -- see
# release.yml, which sets this for the macOS release job only.
if [[ "$TARGET_EXEC" != "-" && -z "$BIALET_SKIP_CRON_RELOAD_TEST" ]]; then
  cron_line=$LINENO
  _test_start_ms=$(now_ms)
  cron_root=$(mktemp -d)
  rm -f /tmp/tests-cron.log
  require_free_port "$HOST" "$CRON_PORT" "cron reload server"
  "$TARGET_EXEC" -h "$HOST" -p "$CRON_PORT" -l /tmp/tests-cron.log \
    "$cron_root" > /dev/null 2>&1 &
  disown
  wait_port "$HOST" "$CRON_PORT" 10
  cron_installs_before=$(grep -c "Installing cron" /tmp/tests-cron.log 2>/dev/null)
  cron_installs_before=${cron_installs_before:-0}
  # 30s per phase: on GitHub's macOS runners, FSEvents delivery for a
  # freshly-created ephemeral volume/dir has been observed to lag well past
  # 10s (the create/edit installs still land, just later), which flaked this
  # test even though the underlying reinstall logic was correct.
  printf 'Cron.every(1) { |d| System.print("CRON-MARKER") }\n' > "$cron_root/_cron.wren"
  cron_installs_create=0
  cron_deadline=$(( $(now_ms) + 30000 ))
  while [[ $cron_installs_create -le $cron_installs_before && $(now_ms) -lt $cron_deadline ]]; do
    cron_installs_create=$(grep -c "Installing cron" /tmp/tests-cron.log 2>/dev/null)
    cron_installs_create=${cron_installs_create:-0}
    [[ $cron_installs_create -le $cron_installs_before ]] && sleep 1
  done
  printf 'Cron.every(1) { |d| System.print("CRON-MARKER-EDIT") }\n' > "$cron_root/_cron.wren"
  cron_installs_edit=$cron_installs_create
  cron_deadline=$(( $(now_ms) + 30000 ))
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
elif [[ -n "$BIALET_SKIP_CRON_RELOAD_TEST" ]]; then
  skip_test "Cron reload on file create/edit" "BIALET_SKIP_CRON_RELOAD_TEST set (flaky FSEvents delivery on this runner)"
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
  require_free_port "$HOST" "$DEV_PORT" "bialet dev server"
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
