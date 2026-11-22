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
ROOT_STDERR_PORT="${args[8]:-7105}"

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
  test_syntax "get/set as member names pass    " "syntax_ok_get.wren" 0
  test_syntax "getter + method with args pass " "syntax_ok_member_overload.wren" 0
  test_syntax_msg "get keyword names the getter  " "syntax_err_get_keyword.wren" 1 "Wren has no 'get' declaration. Write the getter as 'isDueToday { ... }' or the method as 'isDueToday() { ... }'."
  # A getter and a zero-argument method of the same name make the no-paren
  # spelling ambiguous, so the second declaration is a compile error in either
  # order, and for statics too.
  test_syntax_msg "Getter then zero-arg method     " "syntax_err_dupe_member_getter_first.wren" 1 "Class Example already defines a getter 'prop'. A zero-argument method 'prop()' cannot share its name."
  test_syntax_msg "Zero-arg method then getter     " "syntax_err_dupe_member_method_first.wren" 1 "Class Example already defines a method 'met()'. A getter 'met' cannot share its name."
  test_syntax_msg "Static getter + method conflict " "syntax_err_dupe_member_static.wren" 1 "Class Example already defines a static getter 'total'. A zero-argument method 'total()' cannot share its name."
  test_syntax "Syntax import passes w/ root   " "syntax_import_ok.wren"  0 "$(dirname "$0")"
  test_syntax "Syntax import fail w/ root    " "syntax_import_err.wren" 1 "$(dirname "$0")"
  test_syntax "Deep nested tags pass         " "syntax_ok_deepnest.wren" 0
  test_syntax "Multiline interp passes       " "syntax_ok_interp_multiline.wren" 0
  test_syntax "Nested same tag passes        " "syntax_ok_nested.wren" 0
  test_syntax "Unclosed nested tag fails     " "syntax_err_nest_unterminated.wren" 1
  test_syntax "Stray close tag fails         " "syntax_err_nest_stray_close.wren" 1
  test_syntax "Mismatched nested close fails " "syntax_err_nest_mismatched.wren" 1
  test_syntax "Cut opening tag fails         " "syntax_err_nest_cut_tag.wren" 1
  test_syntax "Unclosed after interp fails   " "syntax_err_nest_interp_unterminated.wren" 1
  # The next four pin the wording of the nesting errors: the tag name, how many
  # copies are open and closed, and (for a literal split by {{ }}) the line the
  # literal started on rather than the line it resumed on.
  test_syntax_msg "Unclosed counts open/closed   " "syntax_err_nest_unterminated.wren" 1 "Unclosed <div>: 2 open, 1 closed (string opened on line 1)"
  test_syntax_msg "Unclosed names the wrong close" "syntax_err_nest_mismatched.wren" 1 "Unclosed <div>: 2 open, 1 closed. </span> does not close <div>"
  test_syntax_msg "Unclosed names the start line " "syntax_err_nest_interp_unterminated.wren" 1 "Unclosed <div>: 2 open, 0 closed (string opened on line 1)"
  test_syntax_msg "Cut opening tag names the tag " "syntax_err_nest_cut_tag.wren" 1 "Expected '>' to close the opening <div> tag."
  test_syntax_msg "Cut attributes report the tag" "syntax_err_nest_cut_attrs.wren" 1 "Expected '>' to close the opening <div> tag."
  # A complete opening tag that never closes is still the Unclosed message, and
  # a source ending in a backslash must stop at the end of the file.
  test_syntax_msg "Complete tag still names unclosed" "syntax_err_nest_unclosed_text.wren" 1 "Unclosed <div>: 1 open, 0 closed"
  test_syntax_msg "Trailing backslash at EOF" "syntax_err_string_trailing_escape.wren" 1 "Unterminated string."
  test_syntax_errors "Newline ends an open string " "syntax_err_string_newline.wren" 1 1 "Unterminated string."
  # The error is reported at the line the literal was opened on, and the
  # swallowed newline is not counted twice for the tokens after it.
  if [[ "$TARGET_EXEC" != "-" ]]; then
    strline_line=$LINENO
    # The "module line N" prefix is capped at MAX_LINE_ERROR_LEN (100 bytes) in
    # bialet_wren_error(), so a deep checkout path pushes the line number out
    # of the printed message. Run the file under its bare name from a short
    # temp directory instead, so the assertion holds wherever the repo lives.
    strline_dir=$(mktemp -d /tmp/bialet-strline.XXXXXX)
    strline_exe=$(realpath "$TARGET_EXEC")
    cp "$(dirname "$0")/syntax_err_string_line.wren" "$strline_dir/"
    strline_out=$(cd "$strline_dir" &&
      "$strline_exe" -t syntax_err_string_line.wren 2>&1)
    rm -rf "$strline_dir"
    if [[ "$strline_out" == *"line 2 Error: Unterminated string."* ]]; then
      report_result "Open string reports its line " "$strline_line" 0
    else
      report_result "Open string reports its line " "$strline_line" 1 \
        "Expected the error on line 2. Got: $strline_out"
    fi
  else
    skip_test "Open string reports its line " "requires local binary access (-t flag)"
  fi
  test_syntax_msg "Raw string unterminated at EOF" "syntax_err_raw_string_unterminated.wren" 1 "Unterminated raw string."
  # One extra close tag used to leave '</div>' for the parser, which then
  # reported two more errors. It must be a single error.
  test_syntax_errors "Extra close tag, one error    " "syntax_err_nest_stray_close.wren" 1 1 "Unexpected </div>: <div> is already closed."
  test_syntax "Leading op rejected           " "syntax_err_interp_leading_op.wren" 1
  test_syntax "Uppercase tag rejected        " "syntax_err_uppercase.wren" 1
  test_syntax_msg "Invalid tag name clear error   " "syntax_err_invalid_tag_upper.wren" 1 "Invalid tag name: must be lowercase alphanumeric + hyphens"
  test_syntax_msg "Invalid tag name clear error   " "syntax_err_invalid_tag_underscore.wren" 1 "Invalid tag name: must be lowercase alphanumeric + hyphens"
else
  skip_test "Syntax validation             " "requires local binary access (-t flag)"
fi

# Tests - -t is compile-only
# It used to wrenInterpret() the file, so a migration ran just by validating it
# and a page importing a layout executed Request.uri at top level, printing a
# runtime "Stack Error" for valid syntax.
if [[ "$TARGET_EXEC" != "-" ]]; then
  vparse_line=$LINENO
  vparse_root=$(mktemp -d)
  if [[ "$TARGET_EXEC" == */* ]]; then
    vparse_exec="$(cd "$(dirname "$TARGET_EXEC")" && pwd)/$(basename "$TARGET_EXEC")"
  else
    vparse_exec="$(command -v "$TARGET_EXEC")"
  fi
  cat > "$vparse_root/_migration.wren" <<'WREN'
Db.migrate("cli_check", "CREATE TABLE IF NOT EXISTS cli_check (id INTEGER)")
WREN
  cat > "$vparse_root/layout.wren" <<'WREN'
var layoutUri = Request.uri
class Layout {
  static render() {
    return <p>{{ layoutUri }}</p>
  }
}
WREN
  cat > "$vparse_root/page.wren" <<'WREN'
import "layout" for Layout
return Layout.render()
WREN
  vparse_mig_out=$(cd "$vparse_root" && "$vparse_exec" -t _migration.wren 2>&1)
  vparse_mig_status=$?
  vparse_page_out=$(cd "$vparse_root" && "$vparse_exec" -t page.wren 2>&1)
  vparse_page_status=$?
  vparse_table=$(cd "$vparse_root" && "$vparse_exec" -q -r \
    'return `SELECT COUNT(*) AS n FROM sqlite_master WHERE name = ?`.val("cli_check")' \
    2>/dev/null)
  rm -rf "$vparse_root"
  if [[ "$vparse_mig_status" -eq 0 && "$vparse_table" == "0" ]]; then
    report_result "-t does not run migrations  " "$vparse_line" 0
  else
    report_result "-t does not run migrations  " "$vparse_line" 1 \
      "Expected exit 0 and no cli_check table, got exit $vparse_mig_status table '$vparse_table' output '$vparse_mig_out'"
  fi
  if [[ "$vparse_page_status" -eq 0 && "$vparse_page_out" != *"Stack Error"* ]]; then
    report_result "-t does not execute imports " "$vparse_line" 0
  else
    report_result "-t does not execute imports " "$vparse_line" 1 \
      "Expected exit 0 without a Stack Error, got exit $vparse_page_status output '$vparse_page_out'"
  fi
else
  skip_test "-t does not run migrations  " "requires local binary access (-t flag)"
  skip_test "-t does not execute imports " "requires local binary access (-t flag)"
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

# Tests - BIALET_TIMEZONE applies to every VM
# The zone used to be installed by Db.init, which only runs in the migration VM;
# each request starts a fresh VM and cron another, so both kept the process zone.
# Now the runtime reads the row once at startup and the Date primitives fall back
# to it. Pacific/Kiritimati is UTC+14: 1700000000 is 2023-11-15 12:13:20 there.
if [[ "$TARGET_EXEC" != "-" && "$IS_WINE" -eq 0 ]]; then
  tz_line=$LINENO
  _test_start_ms=$(now_ms)
  tz_root=$(mktemp -d)
  tz_run=$(mktemp -d)
  cat > "$tz_root/index.wren" <<'WREN'
return "TZREQUEST|%(Date.tz)|%(Date.fromUnix(1700000000))"
WREN
  cat > "$tz_root/_migration.wren" <<'WREN'
Db.migrate("timezone probe", `CREATE TABLE IF NOT EXISTS tz_probe (x INTEGER, createdAt DATETIME DEFAULT CURRENT_TIMESTAMP)`)
System.print("TZMIGRATION|%(Date.tz)|%(Date.fromUnix(1700000000))")
WREN
  cat > "$tz_root/tzdb.wren" <<'WREN'
`INSERT INTO tz_probe (x) VALUES (1)`.query()
var local = `SELECT datetime('now', 'localtime') AS v`.first()["v"]
var stored = `SELECT createdAt FROM tz_probe ORDER BY rowid DESC`.first()["createdAt"]
return "TZDB|%(local)|%(stored)"
WREN
  cat > "$tz_root/_cron.wren" <<'WREN'
System.print("TZCRON|%(Date.tz)|%(Date.fromUnix(1700000000))")
Cron.every(1) { |d| }
WREN
  # Seed the row in a first run: the zone is read at startup, so the server that
  # serves the request has to be a fresh process.
  "$TARGET_EXEC" -q -d "$tz_run/tz.sqlite3" \
    -r 'Config.set("BIALET_TIMEZONE", "Pacific/Kiritimati")' "$tz_root" \
    > /dev/null 2>&1
  tz_log="$tz_run/server.log"
  require_free_port "$HOST" "$CRON_PORT" "timezone server"
  # TZ=UTC on purpose: the log prefix used to print the process zone, so the
  # two zones have to be provably different (Pacific/Kiritimati is UTC+14).
  # The hour is read on both sides of the run so a boundary crossing cannot
  # make the assertion flaky.
  tz_zone_hour_before=$(TZ=Pacific/Kiritimati date +'%Y-%m-%d %H')
  tz_utc_hour_before=$(TZ=UTC date +'%Y-%m-%d %H')
  TZ=UTC "$TARGET_EXEC" -h "$HOST" -p "$CRON_PORT" -d "$tz_run/tz.sqlite3" \
    "$tz_root" > "$tz_log" 2>&1 &
  disown
  wait_port "$HOST" "$CRON_PORT" 10
  tz_request=$(curl -s --max-time 5 "http://$HOST:$CRON_PORT/index")
  tz_db_request=$(curl -s --max-time 5 "http://$HOST:$CRON_PORT/tzdb")
  # The cron thread runs its first pass right after startup; one more second
  # covers a slow scheduler without waiting for the next minute.
  sleep 1
  stop_matching_servers "$TARGET_EXEC -h $HOST -p $CRON_PORT -d $tz_run/tz.sqlite3 $tz_root"
  tz_zone_hour_after=$(TZ=Pacific/Kiritimati date +'%Y-%m-%d %H')
  tz_expected="|Pacific/Kiritimati|2023-11-15 12:13:20"
  if [[ "$tz_request" == "TZREQUEST$tz_expected" ]] \
     && grep -q "TZMIGRATION$tz_expected" "$tz_log" 2>/dev/null \
     && grep -q "TZCRON$tz_expected" "$tz_log" 2>/dev/null; then
    report_result "BIALET_TIMEZONE applies everywhere" "$tz_line" 0
  else
    report_result "BIALET_TIMEZONE applies everywhere" "$tz_line" 1 \
      "Expected '${tz_expected#|}' in request, migration and cron. Got request '$tz_request', log: $(grep -E 'TZMIGRATION|TZCRON' "$tz_log" 2>/dev/null | tr '\n' ' ')"
  fi
  # The "YYYY-MM-DD HH:MM:SS " prefix of a log line follows BIALET_TIMEZONE,
  # not the process zone. The process ran with TZ=UTC, so the first 13 columns
  # must match the configured zone's date and hour and can never match UTC's.
  tz_log_line=$LINENO
  tz_migration_line=$(grep -m1 "TZMIGRATION" "$tz_log" 2>/dev/null)
  tz_log_prefix="${tz_migration_line:0:13}"
  tz_utc_prefix=$(TZ=UTC date +'%Y-%m-%d %H')
  if [[ -n "$tz_log_prefix" ]] \
     && [[ "$tz_log_prefix" == "$tz_zone_hour_before" \
        || "$tz_log_prefix" == "$tz_zone_hour_after" ]] \
     && [[ "$tz_log_prefix" != "$tz_utc_prefix" ]]; then
    report_result "BIALET_TIMEZONE sets the log prefix" "$tz_log_line" 0
  else
    report_result "BIALET_TIMEZONE sets the log prefix" "$tz_log_line" 1 \
      "Expected a log prefix in Pacific/Kiritimati ($tz_zone_hour_before or $tz_zone_hour_after), not UTC ($tz_utc_prefix). Got '${tz_log_prefix}' from '$tz_migration_line'"
  fi
  # SQLite follows the setting through its explicit "localtime" modifier, while
  # what it stores (CURRENT_TIMESTAMP, the default) stays UTC. TZ=UTC on the
  # server makes both halves distinguishable: local must be the zone's date and
  # hour, stored must be UTC's.
  tz_db_line=$LINENO
  tz_db_local=$(printf '%s' "${tz_db_request#TZDB|}")
  tz_db_stored="${tz_db_local##*|}"
  tz_db_local="${tz_db_local%|*}"
  tz_db_local_prefix="${tz_db_local:0:13}"
  tz_db_stored_prefix="${tz_db_stored:0:13}"
  tz_db_ok=0
  if [[ "$tz_db_request" == "TZDB|"* ]]; then
    tz_db_ok=1
    # Each value must land on one of the two hours read around the run, so an
    # hour boundary crossed mid-test cannot make this flaky.
    [[ "$tz_db_local_prefix" == "$tz_zone_hour_before" \
       || "$tz_db_local_prefix" == "$tz_zone_hour_after" ]] || tz_db_ok=0
    [[ "$tz_db_stored_prefix" == "$tz_utc_hour_before" \
       || "$tz_db_stored_prefix" == "$tz_utc_prefix" ]] || tz_db_ok=0
    # ...and neither may be the other zone's hour.
    [[ "$tz_db_local_prefix" != "$tz_utc_hour_before" \
       && "$tz_db_local_prefix" != "$tz_utc_prefix" ]] || tz_db_ok=0
    [[ "$tz_db_stored_prefix" != "$tz_zone_hour_before" \
       && "$tz_db_stored_prefix" != "$tz_zone_hour_after" ]] || tz_db_ok=0
  fi
  if [[ "$tz_db_ok" -eq 1 ]]; then
    report_result "localtime follows the zone, storage stays UTC" "$tz_db_line" 0
  else
    report_result "localtime follows the zone, storage stays UTC" "$tz_db_line" 1 \
      "Expected localtime in Pacific/Kiritimati ($tz_zone_hour_before/$tz_zone_hour_after) and stored UTC ($tz_utc_hour_before/$tz_utc_prefix). Got '$tz_db_request'"
  fi
  rm -rf "$tz_root" "$tz_run"
else
  if [[ "$IS_WINE" -eq 1 ]]; then
    skip_test "BIALET_TIMEZONE applies everywhere" "Windows/Wine has no IANA timezone database"
  else
    skip_test "BIALET_TIMEZONE applies everywhere" "requires a local binary and an IANA timezone database"
  fi
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

# Regression: Win32 strips a trailing dot or space from a path component, so
# "/get.wren." opened get.wren while the suffix test saw "wren." and served the
# module source as a static file. The same spelling bypassed the ignore list
# ("/ignored/data.json."). Whatever the platform does, neither may come back.
wren_alias_line=$LINENO
_test_start_ms=$(now_ms)
wren_alias_ok=1
wren_alias_seen=""
for alias in "get.wren." "get.wren%20" "get.wren::\$DATA" "ignored/data.json."; do
  body=$(curl -s --path-as-is --max-time 10 "http://$HOST:$PORT/$alias?foo=bar")
  wren_alias_seen+="$alias:'${body:0:40}' "
  [[ "$body" != *"Request.get"* && "$body" != *"\"ignored\": true"* ]] || wren_alias_ok=0
done
if [[ "$wren_alias_ok" == 1 ]]; then
  report_result "Win32 name alias no source   " "$wren_alias_line" 0
else
  report_result "Win32 name alias no source   " "$wren_alias_line" 1 \
    "Expected no module source and no ignored file. Got $wren_alias_seen"
fi

# Static files of every type say nosniff (only .html did), matched
# case-insensitively, and a 414 carries its reason phrase.
static_hdr_line=$LINENO
_test_start_ms=$(now_ms)
static_txt=$(curl -sI --max-time 5 "http://$HOST:$PORT/test.txt" | tr -d '\r')
static_long=$(curl -s -o /dev/null -D - --max-time 5 \
  "http://$HOST:$PORT/$(printf 'L%.0s' $(seq 1 2100))" | head -1 | tr -d '\r')
# The header buffer is BUFSIZ * 4: 32 KiB on glibc but 2 KiB on Windows, where
# a target that would reach the 414 check overflows the header block first and
# is refused with 400.
static_long_ok=0
[[ "$static_long" == "HTTP/1.1 414 URI Too Long" ]] && static_long_ok=1
[[ "$IS_WINE" == 1 && "$static_long" == "HTTP/1.1 400 Bad Request" ]] && static_long_ok=1
if [[ "$static_txt" == *"X-Content-Type-Options: nosniff"* && "$static_long_ok" == 1 ]]; then
  report_result "Static nosniff, 414 phrase   " "$static_hdr_line" 0
else
  report_result "Static nosniff, 414 phrase   " "$static_hdr_line" 1 \
    "Expected nosniff on /test.txt and 'HTTP/1.1 414 URI Too Long'. Got '${static_txt//$'\n'/ }' and '$static_long'"
fi

run_test "Private-looking query served" "get?foo=bar&next=/_x/.y" 200 "bar"
run_test "This URL not exists         " "donotexists"     404
run_test "Custom 404 error page       " "donotexists"     404 "custom-404-page"
run_test "Check HTTP method           " "method-check"    200 "GET"

# A .wren error page is executed when present, and it keeps the error status
# unless it calls Response.status() itself. Regression: bialet_run() defaults
# the status to 200, and custom_error() used that default, so a 404.wren made
# every 404 look like a successful page. Needs its own root: the shared tests/
# app must keep its static 404.html for the assertions above.
if [[ "$TARGET_EXEC" != "-" ]]; then
  errpage_line=$LINENO
  _test_start_ms=$(now_ms)
  errpage_root=$(mktemp -d)
  printf 'return <div><h1>WREN-404-PAGE</h1></div>\n' > "$errpage_root/404.wren"
  printf 'Response.status(418)\nreturn <div><h1>WREN-500-PAGE</h1></div>\n' > "$errpage_root/500.wren"
  printf 'return "ok"\n' > "$errpage_root/index.wren"
  printf 'var value = null\nreturn value.get("missing")\n' > "$errpage_root/boom.wren"
  require_free_port "$HOST" "$CRON_PORT" "error page server"
  "$TARGET_EXEC" -h "$HOST" -p "$CRON_PORT" "$errpage_root" > /dev/null 2>&1 &
  disown
  wait_port "$HOST" "$CRON_PORT" 10
  errpage_404=$(curl -s -o /tmp/bialet-errpage-404 -w "%{http_code}" \
    "http://$HOST:$CRON_PORT/missing-page")
  errpage_500=$(curl -s -o /tmp/bialet-errpage-500 -w "%{http_code}" \
    "http://$HOST:$CRON_PORT/boom")
  stop_matching_servers "$TARGET_EXEC -h $HOST -p $CRON_PORT $errpage_root"
  rm -rf "$errpage_root"
  if [[ "$errpage_404" == "404" ]] \
     && grep -q "WREN-404-PAGE" /tmp/bialet-errpage-404 2>/dev/null \
     && [[ "$errpage_500" == "418" ]] \
     && grep -q "WREN-500-PAGE" /tmp/bialet-errpage-500 2>/dev/null; then
    report_result "Wren error pages keep status" "$errpage_line" 0
  else
    report_result "Wren error pages keep status" "$errpage_line" 1 \
      "Expected 404/WREN-404-PAGE and 418/WREN-500-PAGE. Got $errpage_404:'$(cat /tmp/bialet-errpage-404 2>/dev/null | head -c 80)' $errpage_500:'$(cat /tmp/bialet-errpage-500 2>/dev/null | head -c 80)'"
  fi
  rm -f /tmp/bialet-errpage-404 /tmp/bialet-errpage-500
else
  skip_test "Wren error pages keep status" "requires local binary access"
fi

# User-file caching is opt-in (-x); the framework module is cached either way.
# Without -x no user file may ever be served from a stale compile; with -x an
# edited served file must be noticed on the next request (source digest
# mismatch), never served from the previous blob.
if [[ "$TARGET_EXEC" != "-" ]]; then
  cache_line=$LINENO
  _test_start_ms=$(now_ms)
  cache_root=$(mktemp -d)
  cache_ok=1
  cache_detail=""
  run_cache_case() {
    local flag=$1 first=$2 second=$3
    local args=()
    [[ -n "$flag" ]] && args+=("$flag")
    printf 'return "%s"\n' "$first" > "$cache_root/index.wren"
    require_free_port "$HOST" "$CRON_PORT" "cache server"
    "$TARGET_EXEC" "${args[@]}" -h "$HOST" -p "$CRON_PORT" "$cache_root" \
      > /dev/null 2>&1 &
    disown
    wait_port "$HOST" "$CRON_PORT" 10
    local got_first got_second got_touched
    got_first=$(curl -s --max-time 5 "http://$HOST:$CRON_PORT/")
    printf 'return "%s"\n' "$second" > "$cache_root/index.wren"
    got_second=$(curl -s --max-time 5 "http://$HOST:$CRON_PORT/")
    # A touch (new mtime, same bytes) must keep the old content: on the cached
    # path the signature misses, the digest matches and the stored signature is
    # refreshed; with -x off the file is simply read again.
    touch "$cache_root/index.wren"
    got_touched=$(curl -s --max-time 5 "http://$HOST:$CRON_PORT/")
    pgrep -f -- "$TARGET_EXEC.*-p $CRON_PORT $cache_root" \
      2>/dev/null | xargs -I {} kill -9 {} 2>/dev/null
    # Let the port go before the next case starts a server on it.
    for _ in $(seq 1 20); do
      check_port "$HOST" "$CRON_PORT" 0.2 || break
      sleep 0.1
    done
    [[ "$got_first" == "$first" && "$got_second" == "$second" \
       && "$got_touched" == "$second" ]] || {
      cache_ok=0
      cache_detail="flag:'$flag' expected '$first' then '$second' twice, got '$got_first', '$got_second', '$got_touched'"
    }
  }
  run_cache_case "" "cache-A" "cache-B"
  run_cache_case "-x" "cache-A" "cache-B"
  rm -rf "$cache_root"
  if [[ "$cache_ok" == "1" ]]; then
    report_result "Served file cache invalidation" "$cache_line" 0
  else
    report_result "Served file cache invalidation" "$cache_line" 1 "$cache_detail"
  fi
else
  skip_test "Served file cache invalidation" "requires local binary access"
fi

# The two embedded modules (wren_core.wren and bialet.wren) are replayed from
# blobs, never recompiled, once the process has one of each. Nothing about that
# is visible from outside -- the blobs never leave the process -- so the binary
# carries two test hooks for it: BIALET_CACHE_TRACE logs one line per embedded
# module saying "replayed" or "compiled", and BIALET_CACHE_FAULT stores one
# truncated copy of the framework blob so the next VM has to reject it.
#
# The parent process boots one VM before it forks the HTTP child, so by the time
# the port answers both blobs exist and every request below is a later VM. That
# is exactly the case R1 flagged: if the second VM silently missed, these tests
# would see "compiled" and fail.
if [[ "$TARGET_EXEC" != "-" ]]; then
  replay_line=$LINENO
  _test_start_ms=$(now_ms)
  replay_root=$(mktemp -d)
  replay_log=$(mktemp)
  replay_ok=1
  replay_detail=""
  printf 'return "replay-A"\n' > "$replay_root/index.wren"

  # Starts the server with [fault] (0/1) and leaves "port ready" as the state
  # where the parent has already stored its blobs and every later line in
  # $replay_log comes from a request.
  start_replay_server() {
    local fault=$1
    : > "$replay_log"
    require_free_port "$HOST" "$CRON_PORT" "replay server"
    BIALET_CACHE_TRACE=1 BIALET_CACHE_FAULT="$fault" \
      "$TARGET_EXEC" -h "$HOST" -p "$CRON_PORT" "$replay_root" \
      >> "$replay_log" 2>&1 &
    disown
    wait_port "$HOST" "$CRON_PORT" 10
  }

  stop_replay_server() {
    pgrep -f -- "$TARGET_EXEC.*-p $CRON_PORT $replay_root" \
      2>/dev/null | xargs -I {} kill -9 {} 2>/dev/null
    for _ in $(seq 1 20); do
      check_port "$HOST" "$CRON_PORT" 0.2 || break
      sleep 0.1
    done
  }

  # Lines written after line [from], trimmed to the message text.
  replay_lines_since() {
    tail -n "+$(( $1 + 1 ))" "$replay_log" 2>/dev/null |
      sed -E 's/^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2} //'
  }

  # Requests [count] times, expecting [body] each time, and reports the number of
  # log lines seen before the first request in REPLY.
  replay_requests() {
    local count=$1 body=$2 got i
    REPLY=$(wc -l < "$replay_log")
    for ((i = 0; i < count; i++)); do
      got=$(curl -s --max-time 10 "http://$HOST:$CRON_PORT/")
      if [[ "$got" != "$body" ]]; then
        replay_ok=0
        replay_detail="request $((i + 1)) returned '${got:0:120}'"
        return 1
      fi
    done
    return 0
  }

  # Case 1: the second VM and every later one replays both embedded modules.
  start_replay_server 0
  replay_requests 1 "replay-A"
  replay_before=$REPLY
  replay_requests 1 "replay-A"
  sleep 0.2
  replay_delta=$(replay_lines_since "$replay_before" | grep "Bytecode cache")
  stop_replay_server
  if ! grep -q "Bytecode cache core: replayed" <<< "$replay_delta" ||
     ! grep -q "Bytecode cache framework: replayed" <<< "$replay_delta" ||
     grep -q "compiled" <<< "$replay_delta"; then
    replay_ok=0
    replay_detail="second request did not replay both embedded modules: $replay_delta"
  fi

  # Case 2: a corrupt blob falls back to compiling, leaves the module usable and
  # is replaced; the request after it replays again.
  start_replay_server 1
  replay_requests 1 "replay-A"
  replay_before=$REPLY
  replay_requests 2 "replay-A"
  sleep 0.2
  replay_delta=$(replay_lines_since "$replay_before" | grep "Bytecode cache")
  stop_replay_server
  if ! grep -q "Discarding a corrupt cached module" <<< "$replay_delta" ||
     ! grep -q "framework: compiled" <<< "$replay_delta" ||
     ! grep -q "framework: replayed" <<< "$replay_delta"; then
    replay_ok=0
    replay_detail="corrupt blob did not fall back and recover: $replay_delta"
  fi

  # A maskable mismatch on an embedded blob must never pass silently either; the
  # hook above is a truncation, so this only asserts the message is not printed
  # for a healthy blob.
  if grep -q "no longer fits this VM" "$replay_log"; then
    replay_ok=0
    replay_detail="a healthy embedded blob was reported as a mismatch"
  fi

  rm -rf "$replay_root" "$replay_log"
  if [[ "$replay_ok" == "1" ]]; then
    report_result "Embedded blob replay           " "$replay_line" 0
  else
    report_result "Embedded blob replay           " "$replay_line" 1 "$replay_detail"
  fi
else
  skip_test "Embedded blob replay           " "requires local binary access"
fi

# Http.get built its result list, then allocated the body string into it. A
# body past the VM's GC budget (10 MiB) collected the still-unrooted list and
# the primitive wrote into freed memory: the worker crashed.
if [[ "$TARGET_EXEC" != "-" && "$IS_WINE" == 0 ]]; then
  httpbig_line=$LINENO
  _test_start_ms=$(now_ms)
  httpbig_root=$(mktemp -d)
  httpbig_run=$(mktemp -d)
  head -c 12582912 /dev/zero | tr '\0' 'A' > "$httpbig_root/big.txt"
  require_free_port "$HOST" "$CRON_PORT" "large Http.get server"
  "$TARGET_EXEC" -h "$HOST" -p "$CRON_PORT" -q -d "$httpbig_run/srv.db" \
    "$httpbig_root" > /dev/null 2>&1 &
  disown
  wait_port "$HOST" "$CRON_PORT" 10
  httpbig_exec=$(realpath "$TARGET_EXEC")
  httpbig_out=$(cd "$httpbig_run" && "$httpbig_exec" -r \
    "System.print(\"httpbig:%(Http.get(\"http://$HOST:$CRON_PORT/big.txt\").byteCount_)\")" 2>&1)
  stop_matching_servers "$TARGET_EXEC -h $HOST -p $CRON_PORT -q -d $httpbig_run"
  rm -rf "$httpbig_root" "$httpbig_run"
  if [[ "$httpbig_out" == *"httpbig:12582912"* ]]; then
    report_result "Large Http.get body          " "$httpbig_line" 0
  else
    report_result "Large Http.get body          " "$httpbig_line" 1 \
      "Expected a 12582912-byte body. Got '${httpbig_out:0:200}'"
  fi
else
  skip_test "Large Http.get body          " "requires local binary access (not run under Wine)"
fi

# With -x the served-file cache key is the request path. Each run of slashes
# ("/r", "//r", "///r") used to be a new key that compiled the file again and
# kept one more full blob for the life of the worker, so a client decided how
# much memory the cache held. Spellings must share one entry.
if [[ "$TARGET_EXEC" != "-" ]]; then
  ckey_line=$LINENO
  _test_start_ms=$(now_ms)
  ckey_root=$(mktemp -d)
  ckey_log=$(mktemp)
  printf 'return "ckey-route"\n' > "$ckey_root/r.wren"
  for i in $(seq 1 260); do printf 'return "%s"\n' "$i" > "$ckey_root/f$i.wren"; done
  require_free_port "$HOST" "$CRON_PORT" "cache key server"
  BIALET_CACHE_TRACE=1 "$TARGET_EXEC" -x -h "$HOST" -p "$CRON_PORT" "$ckey_root" \
    >> "$ckey_log" 2>&1 &
  disown
  wait_port "$HOST" "$CRON_PORT" 10
  ckey_bodies=""
  for slashes in / // /// //// ///// /; do
    ckey_bodies+=$(curl -s --path-as-is --max-time 10 "http://$HOST:$CRON_PORT${slashes}r")
  done
  ckey_compiles=$(grep -c "user: compiled" "$ckey_log" 2>/dev/null)
  # And the table is bounded: 260 distinct files never hold more than the
  # ceiling (256 user entries).
  for i in $(seq 1 260); do
    curl -s -o /dev/null --max-time 10 "http://$HOST:$CRON_PORT/f$i"
  done
  stop_matching_servers "$TARGET_EXEC -x -h $HOST -p $CRON_PORT $ckey_root"
  ckey_max=$(sed -n 's/.*user: compiled, entries \([0-9]*\).*/\1/p' "$ckey_log" | sort -n | tail -1)
  rm -rf "$ckey_root" "$ckey_log"
  if [[ "$ckey_bodies" == "$(printf 'ckey-route%.0s' 1 2 3 4 5 6)" \
        && "$ckey_compiles" == "1" && "${ckey_max:-0}" -le 256 \
        && "${ckey_max:-0}" -ge 200 ]]; then
    report_result "Cache key ignores slash runs  " "$ckey_line" 0
  else
    report_result "Cache key ignores slash runs  " "$ckey_line" 1 \
      "Expected six route answers, one compile and at most 256 entries. Got compiles:$ckey_compiles max:$ckey_max bodies:'${ckey_bodies:0:80}'"
  fi
else
  skip_test "Cache key ignores slash runs  " "requires local binary access"
fi
run_test "Response status codes       " "status-codes?code=404" 404 "not found"
run_test "Response status 201         " "status-codes?code=201" 201 "created"

# Every status code get_http_status_description() knows must reach the wire with
# its reason phrase, not the "Unknown Status" fallback. Codes the test app would
# not otherwise emit are driven through the handler's numeric branch; 1xx is out
# because curl treats it as an interim response and waits for a second one.
status_phrase_line=$LINENO
_test_start_ms=$(now_ms)
status_phrase_ok=1
status_phrase_bad=""
for phrase_code in 202 203 204 205 206 300 301 303 304 307 308 \
                   405 406 407 408 409 410 429 501 502 503 504 505 507; do
  phrase_head=$(curl -s -D- -o /dev/null --max-time 5 \
    "http://$HOST:$PORT/status-codes?code=$phrase_code" |
    head -1)
  if [[ "$phrase_head" != "HTTP/1.1 $phrase_code "* \
        || "$phrase_head" == *"Unknown Status"* ]]; then
    status_phrase_ok=0
    status_phrase_bad="$status_phrase_bad $phrase_code:'${phrase_head//$'\r'/}'"
  fi
done
if [[ "$status_phrase_ok" == 1 ]]; then
  report_result "Status reason phrases       " "$status_phrase_line" 0
else
  report_result "Status reason phrases       " "$status_phrase_line" 1 \
    "Expected every probed code to carry its reason phrase. Got:$status_phrase_bad"
fi

run_test "Response headers            " "headers"         200 "headers-set"

# Header field names are case-insensitive (RFC 7230). The Wren map that holds
# them is not, so setting content-type next to the default Content-Type used to
# put two lines on the wire, and a request header looked up with another
# spelling was null.
if [[ "$TARGET_EXEC" != "-" ]]; then
  hcase_line=$LINENO
  _test_start_ms=$(now_ms)
  hcase_hdr=$(curl -s -D - -o /dev/null --max-time 20 -H "x-input: v" \
    "http://$HOST:$PORT/headers-case")
  hcase_body=$(curl -s --max-time 20 -H "x-input: v" \
    "http://$HOST:$PORT/headers-case")
  hcase_xtest=$(printf '%s\n' "$hcase_hdr" | grep -ci '^x-test:')
  hcase_value=$(printf '%s\n' "$hcase_hdr" | awk \
    'tolower($0) ~ /^x-test:/ { gsub(/\r/, ""); print $2; exit }')
  hcase_ctype=$(printf '%s\n' "$hcase_hdr" | grep -ci '^content-type:')
  if [[ "$hcase_body" == "input:v" && "$hcase_xtest" == "1" \
        && "$hcase_value" == "two" && "$hcase_ctype" == "1" ]]; then
    report_result "Header names case-insensitive" "$hcase_line" 0
  else
    report_result "Header names case-insensitive" "$hcase_line" 1 \
      "Expected one X-Test: two and one Content-Type. Got body:'$hcase_body' x-test:$hcase_xtest value:'$hcase_value' content-type:$hcase_ctype"
  fi
else
  skip_test "Header names case-insensitive" "requires local binary access"
fi

# Tests - Request & Response (extended coverage)
run_test "Request uri and query alias " "request-meta?foo=bar" 200 "get|/request-meta|bar"
run_test "Request defaults apply       " "request-meta?mode=defaults&present=value&flag" 200 "fallback|value|fallback|fallback|fallback|true|fallback|fallback|fallback"
run_test "Request public URL           " "request-meta?mode=url&x=1" 200 "http|$HOST:$PORT|http://$HOST:$PORT|http://$HOST:$PORT/x|http://$HOST:$PORT/x|http://$HOST:$PORT/request-meta?mode=url&x=1|http://$HOST:$PORT|http://$HOST:$PORT"
# The Test client always writes Host: localhost, so a custom authority is only
# reachable through curl. It must be reflected and never replaced by
# X-Forwarded-Host.
_test_start_ms=$(now_ms)
url_host_line=$LINENO
url_host_body=$(curl -s -H "Host: example.test:8443" "http://$HOST:$PORT/request-meta?mode=url")
if [[ "$url_host_body" == *"http|example.test:8443|http://example.test:8443|http://example.test:8443/x|"* ]]; then
  report_result "Request public URL uses Host " "$url_host_line" 0
else
  report_result "Request public URL uses Host " "$url_host_line" 1 "Got: $url_host_body"
fi
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
  stop_matching_servers "$TARGET_EXEC -h $HOST -p $MULTIPART_PORT -l /tmp/tests-mdscan.log"
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
# Content-Type as the last header line has no CRLF inside the header region;
# its value lost the final byte, so boundary "zz" was parsed as "z" and a
# "--zX" inside a field value started a new part.
last_body = (b'--zz\r\nContent-Disposition: form-data; name="note"\r\n\r\n'
             b'keep\r\n--zX tail\r\n--zz--\r\n')
head = (b'POST /multipart-detect HTTP/1.1\r\nHost: x\r\nConnection: close\r\n'
        b'Content-Length: ' + str(len(last_body)).encode() + b'\r\n'
        b'Content-Type: multipart/form-data; boundary=zz\r\n\r\n')
s = socket.create_connection((host, port), 10)
s.settimeout(20)
s.sendall(head + last_body)
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
last = 'ok' if b'--zX tail' in data else data.split(b'\r\n\r\n', 1)[-1][:60].decode('utf-8', 'replace')
# A whitespace-only line, a spaced field name and an obs-fold continuation
# are read differently by the C framer and the Wren header map (one ended the
# block early, one skipped the line), so such a block is refused outright.
ws = status(b'Content-Type: text/plain\r\n \r\n'
            b'Content-Type: multipart/form-data; boundary=zz\r\n')
colon = status(b'Content-Type : multipart/form-data; boundary=zz\r\n')
fold = status(b'Content-Type: multipart/form-data;\r\n boundary=zz\r\n')
vt = status(b'X\x0b: hidden\r\n')
print('ws:%d|colon:%d|fold:%d|vt:%d|control:%s|barecr:%d|barelf:%d|last:%s' %
      (ws, colon, fold, vt, control, bare_cr, bare_lf, last))
PY
)
  if [[ "$mpdetect_out" == "ws:400|colon:400|fold:400|vt:400|control:ct:multipart/form-data; boundary=zz|note:injected|file:e.txt:3|barecr:400|barelf:400|last:ok" ]]; then
    report_result "Multipart header agreement    " "$mpdetect_line" 0
  else
    report_result "Multipart header agreement    " "$mpdetect_line" 1 \
      "Expected both parsers to agree on the header block. Got $mpdetect_out"
  fi
else
  skip_test "Multipart header agreement    " "requires local binary access and python3"
fi

# A non-multipart request reaches Wren as a length-framed string. A header value
# with an embedded NUL used to be handed to the VM with wrenSetSlotString(),
# which stops at that byte, so the app saw a truncated value.
if [[ "$TARGET_EXEC" != "-" ]] && command -v python3 >/dev/null 2>&1; then
  nulhdr_line=$LINENO
  _test_start_ms=$(now_ms)
  nulhdr_out=$(python3 - "$HOST" "$PORT" <<'PY'
import socket, sys

host, port = sys.argv[1], int(sys.argv[2])

def exchange(req):
    s = socket.create_connection((host, port), 10)
    s.settimeout(20)
    s.sendall(req)
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
    return data

ok = exchange(b'GET /nul-header HTTP/1.1\r\nHost: x\r\nX-Bin: a\x00bz\r\n'
              b'Connection: close\r\n\r\n')
body = ok.split(b'\r\n\r\n', 1)[1] if b'\r\n\r\n' in ok else b''
# A NUL in the request line is refused: the C router and the length-framed
# Wren map would otherwise disagree on the path.
bad = exchange(b'GET /nul\x00-header HTTP/1.1\r\nHost: x\r\n'
               b'Connection: close\r\n\r\n')
status = int(bad.split(b' ')[1]) if bad.startswith(b'HTTP/') else 0
print(body.decode('utf-8', 'replace').strip() + '|nulreq:%d' % status)
PY
)
  if [[ "$nulhdr_out" == "len:4|hasNul:true|tail:z|nulreq:400" ]]; then
    report_result "NUL-bearing header preserved  " "$nulhdr_line" 0
  else
    report_result "NUL-bearing header preserved  " "$nulhdr_line" 1 \
      "Expected the framed header value, got '$nulhdr_out'"
  fi
else
  skip_test "NUL-bearing header preserved  " "requires local binary access and python3"
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
# C0 (LF, ESC), C1 (CSI U+009B, OSC U+009D in their UTF-8 form), and
# non-shortest forms of the same controls must all be hex-escaped.
body = 'v=abc%0aFORGED%1b%5b31mEND%c2%9bC1%c2%9dos%c0%9bOV%e0%80%80V'
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
print("status:%d|escaped:%s|rawEsc:%s|rawC1:%s|forged:%s" % (
    resp.status,
    b'Log probe abc\\x0aFORGED\\x1b[31mEND\\xc2\\x9bC1\\xc2\\x9dos\\xc0\\x9bOV\\xe0\\x80\\x80V' in log,
    b'\x1b' in log,
    b'\xc2\x9b' in log,
    b'\nFORGED' in log))
PY
)
  if [[ "$applog_out" == "status:200|escaped:True|rawEsc:False|rawC1:False|forged:False" ]]; then
    report_result "App log text escaped         " "$applog_line" 0
  else
    report_result "App log text escaped         " "$applog_line" 1 \
      "Expected one escaped Log record with no raw ESC or C1. Got $applog_out"
  fi
else
  skip_test "App log text escaped         " "requires local binary access and python3"
fi

# Every request target is logged, before the 414 length check, and the escaping
# can quadruple it: a long target must not store a record of its own size.
# 1200 bytes stays inside the smallest header buffer (2 KiB on Windows).
if [[ "$TARGET_EXEC" != "-" ]]; then
  logcap_line=$LINENO
  _test_start_ms=$(now_ms)
  curl -s -o /dev/null --max-time 10 \
    "http://$HOST:$PORT/LOGCAPMARK$(printf 'B%.0s' $(seq 1 1200))"
  # tr -d '\r': the Windows build writes CRLF line endings.
  logcap_len=$(grep "LOGCAPMARK" /tmp/tests.log 2>/dev/null | tail -1 | tr -d '\r' | wc -c | tr -d ' ')
  logcap_tail=$(grep "LOGCAPMARK" /tmp/tests.log 2>/dev/null | tail -1 | tr -d '\r' | tail -c 4)
  if [[ "${logcap_len:-0}" -gt 0 && "$logcap_len" -lt 700 && "$logcap_tail" == "..." ]]; then
    report_result "Logged request target capped" "$logcap_line" 0
  else
    report_result "Logged request target capped" "$logcap_line" 1 \
      "Expected one record under 700 bytes ending in '...'. Got length:$logcap_len tail:'$logcap_tail'"
  fi
else
  skip_test "Logged request target capped" "requires local binary access"
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
  stop_matching_servers "$TARGET_EXEC -h $HOST -p $MULTIPART_PORT -l /tmp/tests-multipart.log"
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
  cap_many="skipped"
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
    # MAX_UPLOAD_FILES caps one request at 100 file parts. The 101st and later
    # are skipped instead of inserting rows the per-request cap was meant to
    # bound; text fields after the cap still read.
    cap_many_body="$(mktemp)"
    python3 -c "
parts = []
for i in range(150):
    parts.append('--zz\r\nContent-Disposition: form-data; name=\"g%d\"; filename=\"z\"\r\n\r\n\r\n' % i)
open('$cap_many_body', 'w').write(''.join(parts) + '--zz--\r\n')"
    cap_many=$(curl -s --max-time 20 \
      -H "Content-Type: multipart/form-data; boundary=zz" \
      --data-binary "@$cap_many_body" \
      "http://$HOST:$MULTIPART_PORT/multipart-files")
    rm -f "$cap_many_body"
  fi
  stop_matching_servers "$TARGET_EXEC -h $HOST -p $MULTIPART_PORT -l /tmp/tests-cap.log"
  rm -f "$cap_body" "$cap_db" "$cap_db"-*
  if [[ "$cap_first" == "200" && "$cap_last" == "200" \
        && "$cap_usage" == "rows:4096|index:true|scan:|evict:true" \
        && "$cap_legit" == "200" && "$cap_big" == "200" \
        && "$cap_many" == "files:100" ]]; then
    report_result "Upload ceiling bounds rows     " "$cap_line" 0
  else
    report_result "Upload ceiling bounds rows     " "$cap_line" 1 \
      "Expected every fill and the later upload to answer 200 with the pool bounded at 4096 rows from the index, and a 150-file request to retain 100. Got first:$cap_first last:$cap_last usage:'$cap_usage' legit:$cap_legit big:$cap_big many:'$cap_many'"
  fi
else
  skip_test "Upload ceiling bounds rows     " "requires local binary access"
fi

# Regression: the -l file receives every request URI (query strings carry
# tokens) and all System.log output, but only the -d path was checked against
# the served tree. A public name inside the app root was downloadable by any
# client; startup now refuses it, exactly like -d.
if [[ "$TARGET_EXEC" != "-" ]]; then
  log_guard_line=$LINENO
  _test_start_ms=$(now_ms)
  log_guard_db="$(mktemp /tmp/bialet-logguard.XXXXXX)"
  log_guard_ok=1
  log_guard_seen=""
  # A public name, and a name under .well-known/: the router serves that
  # subtree, so the guard must refuse it even though it starts with '.'.
  for log_guard_file in "$(dirname "$0")/inside-root.log" \
                        "$(dirname "$0")/.well-known/inside-root.log"; do
    rm -f "$log_guard_file"
    require_free_port "$HOST" "$MULTIPART_PORT" "log path guard server"
    "$TARGET_EXEC" -h "$HOST" -p "$MULTIPART_PORT" -l "$log_guard_file" \
      -d "$log_guard_db" "$(dirname "$0")" > /tmp/tests-logguard.out 2>&1 &
    log_guard_pid=$!
    for _ in $(seq 1 20); do
      kill -0 "$log_guard_pid" 2>/dev/null || break
      sleep 0.1
    done
    if kill -0 "$log_guard_pid" 2>/dev/null; then
      log_guard_state="started"
      stop_matching_servers "$TARGET_EXEC -h $HOST -p $MULTIPART_PORT -l $log_guard_file"
    else
      wait "$log_guard_pid" 2>/dev/null
      log_guard_state="exit:$?"
    fi
    stop_matching_servers "$TARGET_EXEC -h $HOST -p $MULTIPART_PORT -l $log_guard_file"
    rm -f "$log_guard_file"
    if [[ "$log_guard_state" != "exit:11" ]] ||
       ! grep -q "log file inside the app root" /tmp/tests-logguard.out; then
      log_guard_ok=0
      log_guard_seen+="${log_guard_file##*/tests/} -> $log_guard_state: $(head -c 120 /tmp/tests-logguard.out); "
    fi
  done
  rm -f "$log_guard_db" "$log_guard_db"-*
  if [[ "$log_guard_ok" == 1 ]]; then
    report_result "Log path inside app root      " "$log_guard_line" 0
  else
    report_result "Log path inside app root      " "$log_guard_line" 1 \
      "Expected exit 11 and a refusal on stderr. Got $log_guard_seen"
  fi
else
  skip_test "Log path inside app root      " "requires local binary access"
fi

# -m above -M (or above an inherited address-space limit) used to make
# setrlimit() fail in every child, so the supervisor restarted it forever and
# nothing was ever served. The pair is now clamped like the CPU limits.
if [[ "$TARGET_EXEC" != "-" ]]; then
  memclamp_line=$LINENO
  _test_start_ms=$(now_ms)
  memclamp_db="$(mktemp /tmp/bialet-memclamp.XXXXXX)"
  require_free_port "$HOST" "$CRON_PORT" "memory clamp server"
  "$TARGET_EXEC" -h "$HOST" -p "$CRON_PORT" -m 1024 -M 256 -d "$memclamp_db" \
    -l /tmp/tests-memclamp.log "$(dirname "$0")" > /dev/null 2>&1 &
  disown
  wait_port "$HOST" "$CRON_PORT" 10
  memclamp_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 \
    "http://$HOST:$CRON_PORT/get?foo=bar")
  stop_matching_servers "$TARGET_EXEC -h $HOST -p $CRON_PORT -m 1024 -M 256"
  memclamp_restarts=$(grep -c "Restarting" /tmp/tests-memclamp.log 2>/dev/null)
  rm -f "$memclamp_db" "$memclamp_db"-* /tmp/tests-memclamp.log
  if [[ "$memclamp_code" == "200" && "${memclamp_restarts:-0}" == "0" ]]; then
    report_result "Memory limits clamped       " "$memclamp_line" 0
  else
    report_result "Memory limits clamped       " "$memclamp_line" 1 \
      "Expected 200 and no restarts with -m 1024 -M 256. Got code:$memclamp_code restarts:$memclamp_restarts"
  fi
else
  skip_test "Memory limits clamped       " "requires local binary access"
fi

# A worker killed by the request it serves (here its 1 s CPU limit) is
# restarted, however often it happens. The supervisor used to count deaths
# within two seconds of a fork as startup failures and stopped for good after
# ten, so ten quick requests ended the server.
if [[ "$TARGET_EXEC" != "-" && "$IS_WINE" == 0 ]]; then
  cpukill_line=$LINENO
  _test_start_ms=$(now_ms)
  cpukill_root=$(mktemp -d)
  cpukill_db=$(mktemp /tmp/bialet-cpukill.XXXXXX)
  printf 'return "alive"\n' > "$cpukill_root/index.wren"
  printf 'var i = 0\nwhile (true) { i = i + 1 }\n' > "$cpukill_root/spin.wren"
  require_free_port "$HOST" "$CRON_PORT" "worker restart server"
  "$TARGET_EXEC" -h "$HOST" -p "$CRON_PORT" -q -c 1 -C 2 -d "$cpukill_db" \
    "$cpukill_root" > /dev/null 2>&1 &
  disown
  wait_port "$HOST" "$CRON_PORT" 10
  for _ in $(seq 1 11); do
    curl -s -o /dev/null --max-time 10 "http://$HOST:$CRON_PORT/spin"
  done
  sleep 0.5
  cpukill_after=$(curl -s --max-time 5 "http://$HOST:$CRON_PORT/")
  stop_matching_servers "$TARGET_EXEC -h $HOST -p $CRON_PORT -q -c 1 -C 2"
  rm -rf "$cpukill_root" "$cpukill_db" "$cpukill_db"-*
  if [[ "$cpukill_after" == "alive" ]]; then
    report_result "Killed workers keep restarting" "$cpukill_line" 0
  else
    report_result "Killed workers keep restarting" "$cpukill_line" 1 \
      "Expected the server to answer after 11 worker kills. Got '$cpukill_after'"
  fi
else
  skip_test "Killed workers keep restarting" "requires local binary access (no supervisor on Windows)"
fi

# The -l log records request targets (tokens in query strings): a new file is
# created readable by its owner only.
if [[ "$TARGET_EXEC" != "-" && "$IS_WINE" == 0 ]]; then
  logperm_line=$LINENO
  _test_start_ms=$(now_ms)
  logperm_dir=$(mktemp -d)
  "$TARGET_EXEC" -l "$logperm_dir/new.log" -v > /dev/null 2>&1
  logperm=$(ls -l "$logperm_dir/new.log" 2>/dev/null | cut -c1-10)
  rm -rf "$logperm_dir"
  if [[ "$logperm" == "-rw-------" ]]; then
    report_result "New log file is 0600        " "$logperm_line" 0
  else
    report_result "New log file is 0600        " "$logperm_line" 1 "Expected -rw-------. Got '$logperm'"
  fi
else
  skip_test "New log file is 0600        " "requires local binary access (POSIX modes)"
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
run_test "Session cap spares authed  " "session-evict" 200 "authed:yes|evicted:yes"
run_test "Session idle expiry       " "session-expiry" 200 "active:yes|idle:yes"
run_test "Session destroy scope      " "session-destroy-scope" 200 "victim:kept|marker:kept|self:gone"
run_test "Session reads follow cookie" "session-stale" 200 "before:v|rotated:null|idMatches:true|destroyed:null|noId:true"

# Regression: the sweep counter lived in a Wren class variable, but bialet_run()
# builds and frees a VM per request, so the counter restarted at 0 and never
# reached its threshold. One write per request (Session.csrf) therefore grew
# BIALET_SESSION by a row per request forever. With maxSessions = 1 spread over
# many requests, the live id count must stay far below the request count, which
# is only possible if the sweep now runs across requests.
session_bound_line=$LINENO
_test_start_ms=$(now_ms)
session_bound_requests=800
session_bound_urls=()
for _ in $(seq 1 "$session_bound_requests"); do
  session_bound_urls+=("http://$HOST:$PORT/session-bound")
done
curl -s --max-time 120 "${session_bound_urls[@]}" >/dev/null 2>&1
session_bound_live=$(curl -s --max-time 10 \
  "http://$HOST:$PORT/session-bound?report=1")
if [[ "$session_bound_live" =~ ^[0-9]+$ ]] &&
   (( session_bound_live < session_bound_requests )); then
  report_result "Session bound across requests" "$session_bound_line" 0
else
  report_result "Session bound across requests" "$session_bound_line" 1 \
    "Expected fewer than $session_bound_requests live session ids, got '$session_bound_live'"
fi
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
run_test "Same tag nesting            " "html-nest.wren"  200 "$(read_file "html-nest.html")"
run_test "Literal text with <          " "html-lt-text"   200 "<main><p>a<b c</p><p>1 < 2 and 3 > 2</p></main>"
run_test "Nested attr interpolation    " "html-nest-attr-interp" 200 '<main><div><div style="width:&lt;/b&gt;">a</div><div>b</div></div><div><div title=true>x</div></div><div style="&lt;/div&gt;">y</div></main>'
run_test "Multiline call arguments    " "html-multiline-args" 200 "three:same"
run_test "Newline after HTML literal  " "html-literal-newline" 200 "badge:ok"
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
run_test "Query arity up to seven     " "query-arity"     200 "1234567|1|1|7|true|true"
run_test "Query first keeps LIMIT     " "query-limit"     200 "1|false|false|false|false|false|true|LIMIT|1"
run_test "Query non-primitive params  " "query-param-stringify" 200 "node-name|tail|node-name"
run_test "Query to(Class) mapping     " "db-to-class"     200 "alpha:10,beta:20"
run_test "Query RETURNING clause      " "db-returning"    200 "5,5,5"
run_test "Db save delete migrate      " "db-more"         200 "inserted:"
run_test "Db delete validates table     " "db-delete-guard" 200 "literal:1|aborted:true|intact:1|other:2|refused:false|session:1"
run_test "Query binds reject NUL bytes  " "nul-bind" 200 "paramAborted:true|textAborted:true|bindAborted:true|control:admin"
run_test "For-in rejects non-instances   " "iterate-guard" 200 "bool:true|num:true|null:true|missing:true|rowValues:true|rowKeys:true|fromString:true|fetchFromString:true|saveBool:true|saveMissing:true|control:9"
run_test "NULL column maps to null     " "db-null"         200 "literal:true|bound:true"
run_test "UTF-8 range slices stay intact" "utf8-range" "v=café" 200 "lit:true|litHex:63,61,66,c3,a9|emoji:true|emojiHex:61,f0,9f,98,80,62|reverseHex:c3,a9,66,61,63|reverse:true|tailHex:a9|midHex:f0,9f,98,80|split:true|splitHex:64,c3,b3,73|trim:true|trimHex:73,65,c3,b1,6f,72|replace:true|replaceHex:63,61,66,c3,a9,20,73,69,6e,20,6c,65,63,68,65|posted:true|postedHex:63,61,66,c3,a9"
run_test "UTF-8 code point above max   " "utf8-codepoint" 200 "same:yes|byte:244|iter:yes"

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
  run_test "Http custom credential strip  " "http-redirect?which=strip&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "secret|no-auth|no-auth|no-auth|no-auth|no-auth"
  run_test "Http strip list type-checked " "http-redirect?which=strip-type&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "no-auth|type:true"
  run_test "Http no credential when unset " "http-redirect?which=noauth&target=http://$HOST:$ECHO_PORT" 200 "no-auth"
  run_test "Http reuse keeps per-call auth" "http-redirect?which=reuse&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "here:Basic cmV1c2U6cHc=|other:no-auth|form:a=b|formother:no-k=no-v"
  run_test "Http token not written to map" "http-redirect?which=shared&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "Bearer bearer-shared|no-auth|intact"
  run_test "Http redirect cookie control  " "http-redirect?which=control&target=http://$HOST:$ECHO_PORT" 200 "set|session=abc123"
  run_test "Http cookie jar manual hdr  " "http-options?which=cookie-manual&target=http://$HOST:$ECHO_PORT&other=http://localhost:$ECHO_PORT" 200 "set|manual=1"
  run_test "Http query-string builder   " "http-options?which=query&target=http://$HOST:$ECHO_PORT" 200 "hi there"
  run_test "Http non-string body safe    " "http-options?which=nonstring&target=http://$HOST:$ECHO_PORT" 200 "ok:true|body:"
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
  skip_test "Http non-string body safe    " "$echo_skip_reason"
  skip_test "Http timeout + error message" "$echo_skip_reason"
fi

# Tests - Date & Time
run_test "Date formatting             " "date"            200 "13/09/2024 15:45:30"
run_test "Date now and components     " "date-now"        200 "true"
run_test "Date comparison operators   " "date-compare"    200 "true,true,true,true,true,true,false,true,false,true"
run_test "Date parse from string      " "date-parse"      200 "2024-12-25 14:30:45,UTC"
run_test "Date constructors and get   " "date-more"       200 "dow:5|doy:366|woy:37|diff:86400|fmt:2024-09-13"
run_test "Date from unix timestamp   " "date-unix"       200 "round:2024-09-13 10:30:45|eq:true|zero:true|num:2024-09-13 10:30:45|numTz:UTC|numEq:true"

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

# Tests - Encryption (AES-256-GCM)
run_test "Util encrypt/decrypt        " "crypto"          200 "true"
if [[ "$TARGET_EXEC" != "-" ]]; then
  # Needs the deployment key, which util.sh passes to the server it starts.
  run_test "Config encrypt/decrypt      " "crypto-config"   200 "true"
else
  skip_test "Config encrypt/decrypt      " "server was not started with --secret-key-file"
fi

# Tests - CLI surface
# The option parser is user-facing too: a version flag that exits non-zero, or
# an unknown option that starts a server anyway, is a bug.
if [[ "$TARGET_EXEC" != "-" ]]; then
  cli_line=$LINENO
  _test_start_ms=$(now_ms)
  cli_version=$("$TARGET_EXEC" -v 2>&1)
  cli_version_code=$?
  cli_help=$("$TARGET_EXEC" --help 2>&1)
  cli_help_code=$?
  cli_bad=$("$TARGET_EXEC" --definitely-not-an-option 2>&1)
  cli_bad_code=$?
  cli_no_port=$("$TARGET_EXEC" -p 2>&1)
  cli_no_port_code=$?
  if [[ "$cli_version_code" == 0 && "$cli_version" == *bialet* \
        && "$cli_help_code" == 0 && "$cli_help" == *sage* \
        && "$cli_bad_code" != 0 && "$cli_no_port_code" != 0 ]]; then
    report_result "CLI version and bad options " "$cli_line" 0
  else
    report_result "CLI version and bad options " "$cli_line" 1 \
      "version($cli_version_code):'$cli_version' bad($cli_bad_code):'${cli_bad:0:80}' missing($cli_no_port_code):'${cli_no_port:0:80}'"
  fi
else
  skip_test "CLI version and bad options " "requires local binary access"
fi

# The deployment key loader refuses unsafe files instead of falling back to an
# unencrypted build or reading whatever it was pointed at.
if [[ "$TARGET_EXEC" != "-" ]]; then
  secret_guard_line=$LINENO
  _test_start_ms=$(now_ms)
  secret_root=$(mktemp -d)
  secret_dir=$(mktemp -d)
  secret_missing="$secret_dir/missing"
  secret_empty="$secret_dir/empty"
  : > "$secret_empty"
  secret_big="$secret_dir/big"
  head -c 5000 /dev/zero | tr '\0' 'a' > "$secret_big"
  # Not "nul": on Windows that name is the NUL device, so the test would read
  # \\.\NUL instead of the file it just wrote.
  secret_nul="$secret_dir/nulbyte"
  printf 'ab\0cd\n' > "$secret_nul"
  secret_inside="$secret_root/key.txt"
  printf '%s\n' "00112233445566778899aabbccddeeff" > "$secret_inside"
  secret_guard_ok=1

  secret_case() { # label file expected-text
    local label="$1" file="$2" needle="$3" out code
    out=$("$TARGET_EXEC" -h "$HOST" -p "$CRON_PORT" "$secret_root" -k "$file" 2>&1)
    code=$?
    if [[ "$code" == "0" ]] || [[ "$out" != *"$needle"* ]]; then
      secret_guard_ok=0
      printf '      %s: expected exit 1 with "%s", got %s: %s\n' \
        "$label" "$needle" "$code" "${out//$'\n'/ }"
    fi
  }

  secret_case "missing" "$secret_missing" "secret key file not found"
  secret_case "empty" "$secret_empty" "secret key file is empty"
  secret_case "too large" "$secret_big" "larger than"
  secret_case "NUL byte" "$secret_nul" "NUL byte"
  secret_case "inside root" "$secret_inside" "must live outside the app root"

  rm -rf "$secret_root" "$secret_dir"
  if [[ "$secret_guard_ok" == 1 ]]; then
    report_result "Secret key file guards      " "$secret_guard_line" 0
  else
    report_result "Secret key file guards      " "$secret_guard_line" 1 \
      "One or more unsafe key files was accepted (see the lines above)"
  fi
else
  skip_test "Secret key file guards      " "requires local binary access"
fi

# The other half of the same class of guard: the database path. Each of these
# must be refused at startup with a clear message instead of being opened
# somewhere the static handler can serve it, or being truncated.
if [[ "$TARGET_EXEC" != "-" ]]; then
  db_guard_line=$LINENO
  _test_start_ms=$(now_ms)
  db_guard_ok=1
  db_guard_case() { # label args...
    local label="$1" needle="$2" out code
    shift 2
    out=$("$TARGET_EXEC" "$@" 2>&1)
    code=$?
    if [[ "$code" == "0" ]] || [[ "$out" != *"$needle"* ]]; then
      db_guard_ok=0
      printf '      %s: expected a refusal with "%s", got %s: %s\n' \
        "$label" "$needle" "$code" "${out//$'\n'/ }"
    fi
  }
  db_guard_case "empty" "Database path is empty" -d ""
  # A relative -d is joined onto the root: a trailing slash must be trimmed
  # before that join, and a non-'_' name inside the root must be refused.
  db_guard_case "inside root" "Database inside the app root" -d "data.sqlite3/"
  # An absolute path whose directory does not exist cannot be opened.
  db_guard_case "unopenable" "Can't open database in" \
    -d "/nonexistent-bialet-dir/x.sqlite3"
  # Longer than PATH_MAX on every supported platform: the guard compares the
  # absolute path against a PATH_MAX-sized buffer, and a 1101-byte path fits in
  # Linux's 4096 but not in macOS's 1024, so the old value only exercised the
  # guard on macOS.
  db_guard_long="/$(printf 'a%.0s' $(seq 1 5000))"
  db_guard_case "too long" "Database path too long" -d "$db_guard_long"
  unset db_guard_long
  # A -d whose last component is a dangling symlink into the root: the guard
  # used to resolve only the parent directory, so the server started and the
  # open created a public, downloadable database inside the served tree.
  if [[ "$IS_WINE" == 0 ]]; then
    db_link_root=$(mktemp -d)
    db_link_dir=$(mktemp -d)
    ln -s "$db_link_root/data.sqlite3" "$db_link_dir/link.sqlite3"
    db_link_out=$(mktemp)
    "$TARGET_EXEC" -h "$HOST" -p "$CRON_PORT" -d "$db_link_dir/link.sqlite3" \
      "$db_link_root" > "$db_link_out" 2>&1 &
    db_link_pid=$!
    for _ in $(seq 1 30); do kill -0 "$db_link_pid" 2>/dev/null || break; sleep 0.1; done
    if kill -0 "$db_link_pid" 2>/dev/null; then
      stop_matching_servers "$TARGET_EXEC -h $HOST -p $CRON_PORT -d $db_link_dir"
      db_guard_ok=0
      printf '      dangling symlink: server started instead of refusing\n'
    elif ! grep -q "Database inside the app root" "$db_link_out"; then
      db_guard_ok=0
      printf '      dangling symlink: expected a refusal, got: %s\n' "$(head -c 200 "$db_link_out")"
    fi
    rm -rf "$db_link_root" "$db_link_dir" "$db_link_out"
  fi
  # .well-known/ is served by the router, so a database there is public. The
  # guard used to treat its leading '.' as private and start anyway.
  db_wk_root=$(mktemp -d)
  mkdir -p "$db_wk_root/.well-known"
  db_wk_out=$(mktemp)
  "$TARGET_EXEC" -h "$HOST" -p "$CRON_PORT" -d .well-known/app.db "$db_wk_root" \
    > "$db_wk_out" 2>&1 &
  db_wk_pid=$!
  for _ in $(seq 1 30); do kill -0 "$db_wk_pid" 2>/dev/null || break; sleep 0.1; done
  if kill -0 "$db_wk_pid" 2>/dev/null; then
    stop_matching_servers "$TARGET_EXEC -h $HOST -p $CRON_PORT -d .well-known/app.db"
    db_guard_ok=0
    printf '      .well-known database: server started instead of refusing\n'
  elif ! grep -q "Database inside the app root" "$db_wk_out"; then
    db_guard_ok=0
    printf '      .well-known database: expected a refusal, got: %s\n' "$(head -c 200 "$db_wk_out")"
  fi
  rm -rf "$db_wk_root" "$db_wk_out"
  if [[ "$db_guard_ok" == 1 ]]; then
    report_result "Database path guards        " "$db_guard_line" 0
  else
    report_result "Database path guards        " "$db_guard_line" 1 \
      "One or more unsafe database paths was accepted (see the lines above)"
  fi
else
  skip_test "Database path guards        " "requires local binary access"
fi

# The -T runner's own surface. A failing test file must be reported with its
# line and assertion message, in the plain output and in the -q summary; a
# missing test directory must fail loudly; an empty one must not.
if [[ "$TARGET_EXEC" != "-" ]]; then
  runner_line=$LINENO
  _test_start_ms=$(now_ms)
  runner_root=$(mktemp -d)
  mkdir -p "$runner_root/_tests"
  printf 'Test.assert(1 == 2, "two is one")\n' > "$runner_root/_tests/fail.wren"
  printf 'Test.assert(true, "ok")\n' > "$runner_root/_tests/pass.wren"
  # Run from the parent with a relative directory: read_file() resolves a
  # relative test path before its no-follow open, while an absolute path under
  # macOS' /var or /tmp symlink is refused by that same open.
  runner_exe=$(realpath "$TARGET_EXEC")
  runner_parent=$(dirname "$runner_root")
  runner_name=$(basename "$runner_root")
  runner_quiet=$(cd "$runner_parent" && "$runner_exe" -T "$runner_name" -q 2>&1)
  runner_plain=$(cd "$runner_parent" && "$runner_exe" -T "$runner_name" 2>&1)
  runner_notests_root=$(mktemp -d)
  runner_nodir=$("$TARGET_EXEC" -T "$runner_notests_root" 2>&1)
  runner_nodir_code=$?
  runner_empty_root=$(mktemp -d)
  mkdir -p "$runner_empty_root/_tests"
  runner_empty=$(cd "$runner_parent" &&
    "$runner_exe" -T "$(basename "$runner_empty_root")" 2>&1)
  runner_empty_code=$?
  rm -rf "$runner_root" "$runner_empty_root" "$runner_notests_root"
  if [[ "$runner_quiet" == *"1 of 2 tests failed"* \
        && "$runner_quiet" == *"### FAIL _tests/fail.wren:"* \
        && "$runner_quiet" == *"two is one"* \
        && "$runner_plain" == *"✗ fail.wren"* \
        && "$runner_nodir_code" != 0 \
        && "$runner_nodir" == *"Test directory not found"* \
        && "$runner_empty_code" == 0 \
        && "$runner_empty" == *"No tests found."* ]]; then
    report_result "Test runner output paths    " "$runner_line" 0
  else
    report_result "Test runner output paths    " "$runner_line" 1 \
      "quiet:'${runner_quiet:0:160}' plain:'${runner_plain:0:120}' nodir($runner_nodir_code):'${runner_nodir:0:80}' empty($runner_empty_code):'${runner_empty:0:60}'"
  fi
else
  skip_test "Test runner output paths    " "requires local binary access"
fi

# -t (validate) must refuse a file it cannot resolve, and a file outside the
# application root, instead of validating something else.
if [[ "$TARGET_EXEC" != "-" ]]; then
  validate_line=$LINENO
  _test_start_ms=$(now_ms)
  validate_missing=$("$TARGET_EXEC" -t "$(dirname "$0")/no-such-file.wren" 2>&1)
  validate_missing_code=$?
  validate_outside_file=$(mktemp /tmp/bialet-outside.XXXXXX)
  printf 'var x = 1\n' > "$validate_outside_file"
  validate_outside=$("$TARGET_EXEC" -t "$validate_outside_file" "$(dirname "$0")" 2>&1)
  validate_outside_code=$?
  rm -f "$validate_outside_file"
  if [[ "$validate_missing_code" != 0 && "$validate_missing" == *"Cannot resolve file"* \
        && "$validate_outside_code" != 0 \
        && "$validate_outside" == *"outside the application root"* ]]; then
    report_result "Validate rejects bad paths  " "$validate_line" 0
  else
    report_result "Validate rejects bad paths  " "$validate_line" 1 \
      "missing($validate_missing_code):'${validate_missing:0:80}' outside($validate_outside_code):'${validate_outside:0:80}'"
  fi
else
  skip_test "Validate rejects bad paths  " "requires local binary access"
fi

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

# Tests - documented file serving contracts
run_test "Response.file serves blob   " "file-serve"      200 "served-bytes"
test_method "Response.file content type " "GET" "file-serve" 200 "Content-Type: text/plain" "served-bytes"
test_method "Untrusted file type downloads" "GET" "file-serve-untrusted" 200 \
  "Content-Type: application/octet-stream" "<script>alert(1)</script>"
test_method "Untrusted file sets disposition" "GET" "file-serve-untrusted" 200 \
  "Content-Disposition: attachment" ""
test_method "Responses carry nosniff     " "GET" "get?foo=bar" 200 \
  "X-Content-Type-Options: nosniff" "bar"
run_test "Invalid status is a 500     " "status-guard"    500 "custom-500-page"

# Static files: get_content_type() picks the Content-Type from the extension.
# The fixtures are tiny; only the header matters here.
test_method "Static html content type    " "GET" "static/page.html" 200 \
  "Content-Type: text/html" "page-html"
test_method "Static htm content type     " "GET" "static/page.htm" 200 \
  "Content-Type: text/html" "page-htm"
test_method "Static css content type     " "GET" "static/style.css" 200 \
  "Content-Type: text/css" "color:red"
test_method "Static js content type      " "GET" "static/app.js" 200 \
  "Content-Type: application/javascript" "var js"
test_method "Static jpg content type     " "GET" "static/photo.jpg" 200 \
  "Content-Type: image/jpeg" ""
test_method "Static jpeg content type    " "GET" "static/photo.jpeg" 200 \
  "Content-Type: image/jpeg" ""
test_method "Static png content type     " "GET" "static/icon.png" 200 \
  "Content-Type: image/png" ""
test_method "Static gif content type     " "GET" "static/anim.gif" 200 \
  "Content-Type: image/gif" ""
test_method "Static svg content type     " "GET" "static/logo.svg" 200 \
  "Content-Type: image/svg+xml" ""
test_method "Static txt content type     " "GET" "static/notes.txt" 200 \
  "Content-Type: text/plain" "plain-text"
test_method "Static xml content type     " "GET" "static/data.xml" 200 \
  "Content-Type: application/xml" ""
test_method "Static pdf content type     " "GET" "static/doc.pdf" 200 \
  "Content-Type: application/pdf" ""
test_method "Static no extension type    " "GET" "static/blob" 200 \
  "Content-Type: application/octet-stream" "no-extension"
test_method "Static unknown type         " "GET" "static/blob.unknownext" 200 \
  "Content-Type: application/octet-stream" "mystery"

# The favicon is answered from the embedded blob, before routing and the ignore
# list ever run.
test_method "Favicon served embedded     " "GET" "favicon.ico" 200 \
  "Content-Type: image/x-icon" ""

# "OPTIONS *" (asterisk-form) is legal HTTP but names no path below the app
# root, so the router must refuse it with 400 instead of building a path from
# the root and the bare "*". Old builds served whatever that string happened to
# name.
asterisk_line=$LINENO
_test_start_ms=$(now_ms)
asterisk_status=$(
  exec 3<>"/dev/tcp/$HOST/$PORT" || exit 1
  printf 'OPTIONS * HTTP/1.1\r\nHost: x\r\n\r\n' >&3
  IFS= read -r -t 3 first <&3
  printf '%s' "$first"
)
if [[ "$asterisk_status" == *" 400 "* ]]; then
  report_result "Non-origin target refused   " "$asterisk_line" 0
else
  report_result "Non-origin target refused   " "$asterisk_line" 1 \
    "Expected 400 for 'OPTIONS *'. Got '$asterisk_status'"
fi
# The documented ignore list (README*,AGENTS*,LICENSE*,*.json,*.yml,*.yaml) is
# applied at the final served path, while .well-known stays exempt (tested
# above). The fixtures live in ignored/ so a stray AGENTS.md does not land in
# the repository root where agents and tools would read it.
run_test "Ignored README is 404       " "ignored/README.md"  404 "custom-404-page"
run_test "Ignored AGENTS is 404       " "ignored/AGENTS.md"  404 "custom-404-page"
run_test "Ignored LICENSE is 404      " "ignored/LICENSE"    404 "custom-404-page"
run_test "Ignored json is 404         " "ignored/data.json"  404 "custom-404-page"
run_test "Ignored yml is 404          " "ignored/conf.yml"   404 "custom-404-page"
run_test "Ignored yaml is 404         " "ignored/conf.yaml"  404 "custom-404-page"

# Tests - error pages and recovery
run_test "Runtime error uses 500 page " "throw"           500 "custom-500-page"
run_test "Server alive after an error " "get?foo=bar"     200 "bar"
run_test "404 page is static html     " "donotexists"     404 "custom-404-page"

# Tests - HtmlNode and Markdown
run_test "HtmlNode raw vs escaped     " "htmlnode-raw"    200 '<div><p><b>x</b></p><p>&lt;b&gt;x&lt;/b&gt;</p></div>'
run_test "Markdown headings           " "md-syntax"       200 "<h1>H1</h1>"
run_test "Markdown strong             " "md-syntax"       200 "<strong>bold</strong>"
run_test "Markdown emphasis           " "md-syntax"       200 "<em>italic</em>"
run_test "Markdown unordered list     " "md-syntax"       200 "<li>one</li>"
run_test "Markdown blockquote         " "md-syntax"       200 "<blockquote>"
run_test "Markdown table              " "md-syntax"       200 "<table>"
run_test "Markdown code is escaped    " "md-syntax"       200 "<code>a &lt; b</code>"
run_test "Markdown fenced code        " "md-syntax"       200 "<pre><code>"
run_test "Markdown skips front matter " "md-syntax"       200 "<h1>Body</h1>"
run_test "Markdown list type switch   " "md-syntax"       200 "</ul>
<ol>
<li>b</li>"
run_test "Markdown escapes quotes amp " "md-syntax"       200 "a &amp; b &quot;c&quot; &#39;d&#39;"
run_test "Markdown h3 to h6           " "md-syntax"       200 "<h6>h6</h6>"
run_test "Markdown fenced language    " "md-syntax"       200 "<pre><code>x &lt; y"
run_test "Markdown table then para    " "md-syntax"       200 "</table>
<p>after table</p>"
run_test "Markdown paragraph joins    " "md-syntax"       200 "<p>line one line two</p>"
run_test "Markdown heading ends para  " "md-syntax"       200 "<h1>heading right after</h1>"
run_test "Markdown relative link      " "md-syntax"       200 '<a href="/a/b">rel</a>'
run_test "Markdown uppercase scheme   " "md-syntax"       200 '<a href="HTTP://Example.COM/x">up</a>'
run_test "Markdown rejects bad scheme " "md-syntax"       200 "<p>bad [open](z</p>"
run_test "Markdown drops image scheme " "md-syntax"       200 "dropped ![none]</p>"
run_test "Markdown escapes alt text   " "md-syntax"       200 '<img alt="&quot;alt&quot;" src="y">'
run_test "Markdown image rendered     " "md-syntax"       200 '<img alt="alt" src="pic.png">'
run_test "Markdown quote without space" "md-syntax"       200 "<blockquote>
tight<br>
</blockquote>"
run_test "Markdown trims indent       " "md-syntax"       200 "<p>spaced paragraph</p>"
run_test "Markdown long document      " "md-syntax"       200 "<p>line 1 with some more words to fill it line 2"
run_test "Markdown drops script link  " "md-security"     200 "links:<p>click</p>"
run_test "Markdown escapes code span  " "md-security"     200 "code:<p><code>&lt;b&gt;</code></p>"
run_test "Markdown keeps raw html     " "md-security"     200 "raw:<p><b>raw</b></p>"
run_test "Markdown missing file false " "md-missing"      200 "true"
run_test "Markdown.file reads only .md" "md-file-policy"  200 "true|true|true"
run_test "Log table retention and cap " "log-retention"   200 "old:0|capped:true"
# Request.body joined its lines with "", so every LF of the body was lost.
body_bytes_line=$LINENO
_test_start_ms=$(now_ms)
body_bytes=$(printf 'a=1\r\nb=2\r\n' | curl -s --data-binary @- --max-time 5 "http://$HOST:$PORT/body-bytes")
if [[ "$body_bytes" == "len:10|lf:true|cr:true" ]]; then
  report_result "Request.body keeps newlines " "$body_bytes_line" 0
else
  report_result "Request.body keeps newlines " "$body_bytes_line" 1 \
    "Expected len:10|lf:true|cr:true. Got '$body_bytes'"
fi

# Tests - Query binding limits
run_test "Query four params and paging" "query-pagination" 200 "four:5|page:2|first:3"

# Tests - forms
run_test "Post keeps the last value   " "request-contract" "a=1&a=2" 200 "2|7|true"

# Framing ambiguity must be refused: two Content-Length lines (the C framer and
# the Wren header map disagreed on which one to read) and any
# Transfer-Encoding (chunked bodies are not implemented).
framing_line=$LINENO
_test_start_ms=$(now_ms)
# bash's /dev/tcp keeps this dependency-free and works under Wine too.
framing_dup=$(
  exec 3<>"/dev/tcp/$HOST/$PORT" || exit 1
  printf 'GET /get?foo=bar HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n' >&3
  IFS= read -r -t 3 first <&3
  printf '%s' "$first"
)
framing_te=$(
  exec 3<>"/dev/tcp/$HOST/$PORT" || exit 1
  printf 'GET /get?foo=bar HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n' >&3
  IFS= read -r -t 3 first <&3
  printf '%s' "$first"
)
framing_bad=$(
  exec 3<>"/dev/tcp/$HOST/$PORT" || exit 1
  printf 'POST /get?foo=bar HTTP/1.1\r\nHost: x\r\nContent-Length: 5junk\r\n\r\nhello' >&3
  IFS= read -r -t 3 first <&3
  printf '%s' "$first"
)
if [[ "$framing_dup" == *" 400 "* && "$framing_te" == *" 400 "* \
      && "$framing_bad" == *" 400 "* ]]; then
  report_result "Ambiguous framing refused   " "$framing_line" 0
else
  report_result "Ambiguous framing refused   " "$framing_line" 1 \
    "Expected 400 for duplicate Content-Length, Transfer-Encoding and a malformed Content-Length. Got '$framing_dup', '$framing_te' and '$framing_bad'"
fi

# A HEAD response has the GET head and no content; 204 and 304 have neither
# content nor a Content-Length. All three used to carry the body.
nobody_line=$LINENO
_test_start_ms=$(now_ms)
nobody_raw() {
  (
    exec 3<>"/dev/tcp/$HOST/$PORT" || exit 1
    printf '%s %s HTTP/1.1\r\nHost: x\r\n\r\n' "$1" "$2" >&3
    cat <&3 | tr -d '\r'
  )
}
nobody_head=$(nobody_raw HEAD "/get?foo=bar")
nobody_204=$(nobody_raw GET "/status-body?s=204")
nobody_304=$(nobody_raw GET "/status-body?s=304")
if [[ "$nobody_head" == *"Content-Length: 3" && "$nobody_head" != *bar* \
      && "$nobody_204" == "HTTP/1.1 204 No Content"* && "$nobody_204" != *status-body* \
      && "$nobody_204" != *Content-Length* && "$nobody_304" != *status-body* ]]; then
  report_result "HEAD, 204 and 304 no body    " "$nobody_line" 0
else
  report_result "HEAD, 204 and 304 no body    " "$nobody_line" 1 \
    "Got HEAD='${nobody_head//$'\n'/ }' 204='${nobody_204//$'\n'/ }' 304='${nobody_304//$'\n'/ }'"
fi

# The C router reads the request line with strtok_r(" ") and Request.init
# splits it on every single space, so extra or leading spaces used to reach the
# routed handler with an empty Request.uri or Request.method. Any request line
# that is not exactly "METHOD SP target SP version" is refused.
reqline_line=$LINENO
_test_start_ms=$(now_ms)
reqline_status() {
  (
    exec 3<>"/dev/tcp/$HOST/$PORT" || exit 1
    printf '%s\r\nHost: x\r\n\r\n' "$1" >&3
    IFS= read -r -t 3 first <&3
    printf '%s' "${first%$'\r'}"
  )
}
reqline_bad=""
for reqline in 'GET  /get?foo=bar HTTP/1.1' ' GET /get?foo=bar HTTP/1.1' \
               $'GET\t/get?foo=bar HTTP/1.1' 'GET /get?foo=bar  HTTP/1.1' \
               'GET /get?foo=bar'; do
  got=$(reqline_status "$reqline")
  [[ "$got" == *" 400 "* ]] || reqline_bad+="[$reqline] -> '$got'; "
done
reqline_ok=$(reqline_status 'GET /get?foo=bar HTTP/1.1')
if [[ -z "$reqline_bad" && "$reqline_ok" == *" 200 "* ]]; then
  report_result "Non-canonical request line  " "$reqline_line" 0
else
  report_result "Non-canonical request line  " "$reqline_line" 1 \
    "Expected 400 for every malformed line and 200 for the canonical one. Got: $reqline_bad ok='$reqline_ok'"
fi

# A ".." in the query string is data, not a traversal attempt.
run_test "Dots in query string allowed" "get?foo=.."     200 ".."

# Tests - routing precedence
run_test "Folder route file beats index" "dupe"           200 "dupe-route:root"
run_test "Folder route sees a segment"  "dupe/42"        200 "dupe-route:42"

# Tests - Cookie options and proxy-aware Secure
cookie_opts_line=$LINENO
_test_start_ms=$(now_ms)
cookie_opts_headers=$(curl -s -D- -o /dev/null "http://$HOST:$PORT/cookie-options")
if [[ "$cookie_opts_headers" == *"SameSite=Strict"* ]] \
   && ! printf "%s" "$cookie_opts_headers" | grep -qi '^set-cookie:.*httponly'; then
  report_result "Cookie options override     " "$cookie_opts_line" 0
else
  report_result "Cookie options override     " "$cookie_opts_line" 1 \
    "Expected SameSite=Strict and no HttpOnly. Got: $cookie_opts_headers"
fi

cookie_secure_line=$LINENO
_test_start_ms=$(now_ms)
secure_proxy=$(curl -s -D- -o /dev/null -H "X-Forwarded-Proto: https" \
  "http://$HOST:$PORT/cookie-secure")
secure_plain=$(curl -s -D- -o /dev/null "http://$HOST:$PORT/cookie-secure")
# Forwarded is parsed, not substring-matched: only an exact proto of the first
# element counts.
secure_fwd=$(curl -s -D- -o /dev/null -H 'Forwarded: for=1.2.3.4;proto="https"' \
  "http://$HOST:$PORT/cookie-secure")
secure_fake=$(curl -s -D- -o /dev/null -H "Forwarded: proto=httpsx" \
  "http://$HOST:$PORT/cookie-secure")
# Match the attribute, not the cookie name ("secure-probe" would otherwise
# make the plain response look secure too).
if printf "%s" "$secure_proxy" | grep -qi '^set-cookie:.*; secure' \
   && printf "%s" "$secure_fwd" | grep -qi '^set-cookie:.*; secure' \
   && ! printf "%s" "$secure_fake" | grep -qi '^set-cookie:.*; secure' \
   && ! printf "%s" "$secure_plain" | grep -qi '^set-cookie:.*; secure'; then
  report_result "Cookie Secure follows proxy " "$cookie_secure_line" 0
else
  report_result "Cookie Secure follows proxy " "$cookie_secure_line" 1 \
    "Expected Secure with X-Forwarded-Proto: https and not without. Got: proxy=$secure_proxy plain=$secure_plain"
fi

# Tests - CORS variants
test_method "CORS custom preflight       " "OPTIONS" "cors-custom" 204 \
  "Access-Control-Allow-Methods: GET, POST" ""
test_method "CORS custom headers         " "OPTIONS" "cors-custom" 204 \
  "Access-Control-Allow-Headers: X-Token" ""
run_test "CORS custom GET continues   " "cors-custom"     200 "cors-custom"

# Tests - safe method on basic types
run_test "Safe method basic types     " "safe"            200 "true,42,{html: &lt;b&gt;bold&lt;/b&gt;}"

# Tests - Random
run_test "Random sample               " "randomsample"    200 "true,true,true"

# Regression: a plain "/" request wrote "Path too long to append .wren suffix:
# <stale errno>" to stderr. The trailing-slash strip leaves path ==
# full_root_dir, so the `strlen(path) > root_len` guard failed and the single
# `else` - meant for a path with no room left for ".wren" - called perror() on
# a perfectly normal request. The response was always correct, so this only
# ever showed up in the logs. A URI that really is too long must still be
# reported, once, in the -l stream (not with a stale errno).
if [[ "$TARGET_EXEC" != "-" ]]; then
  quiet_root_line=$LINENO
  _test_start_ms=$(now_ms)
  quiet_root="$(mktemp -d)"
  quiet_root_out="$(mktemp /tmp/bialet-quiet-root.XXXXXX)"
  quiet_root_log="$(mktemp /tmp/bialet-quiet-root-log.XXXXXX)"
  quiet_root_db="$(mktemp /tmp/bialet-quiet-root-db.XXXXXX)"
  printf 'return <h1>Quiet Root</h1>\n' > "$quiet_root/index.wren"
  printf 'return <h1>Quiet Page</h1>\n' > "$quiet_root/page.wren"
  require_free_port "$HOST" "$ROOT_STDERR_PORT" "root stderr server"
  "$TARGET_EXEC" -h "$HOST" -p "$ROOT_STDERR_PORT" -l "$quiet_root_log" \
    -d "$quiet_root_db" "$quiet_root" > "$quiet_root_out" 2>&1 &
  disown
  wait_port "$HOST" "$ROOT_STDERR_PORT" 10
  quiet_root_code=$(curl -s -o /dev/null -w "%{http_code}" \
    "http://$HOST:$ROOT_STDERR_PORT/")
  quiet_root_body=$(curl -s "http://$HOST:$ROOT_STDERR_PORT/")
  quiet_page_code=$(curl -s -o /dev/null -w "%{http_code}" \
    "http://$HOST:$ROOT_STDERR_PORT/page")
  quiet_page_body=$(curl -s "http://$HOST:$ROOT_STDERR_PORT/page")
  quiet_root_noise=$(grep -c "Path too long" "$quiet_root_out" 2>/dev/null)
  # A URI that would not fit in PATH_SIZE is refused with 414 before the router
  # builds a truncated path, and the refusal is logged once. Windows cannot
  # reach that branch at all: its header buffer is BUFSIZ * 4 = 2048 bytes, so
  # any URI that could overflow PATH_SIZE is refused as a malformed request
  # (400) before the probe.
  quiet_long_uri=$(head -c 2100 /dev/zero | tr '\0' 'a')
  quiet_long_code=$(curl -s -o /dev/null -w "%{http_code}" \
    "http://$HOST:$ROOT_STDERR_PORT/$quiet_long_uri")
  # The report has to reach the -l stream and not stderr: that is why it no
  # longer uses perror, and why with -l given it goes to the file alone.
  quiet_long_log=$(grep -c "URI too long" "$quiet_root_log" 2>/dev/null)
  quiet_long_noise=$(grep -c "URI too long" "$quiet_root_out" 2>/dev/null)
  if [[ "$IS_WINE" == 1 ]]; then
    quiet_long_expected_code=400
    quiet_long_expected_log=0
  else
    quiet_long_expected_code=414
    quiet_long_expected_log=1
  fi
  stop_matching_servers "$TARGET_EXEC -h $HOST -p $ROOT_STDERR_PORT"
  rm -rf "$quiet_root" "$quiet_root_out" "$quiet_root_log" "$quiet_root_db" \
    "$quiet_root_db"-*
  if [[ "$quiet_root_code" == "200" && "$quiet_root_body" == *"Quiet Root"* \
        && "$quiet_page_code" == "200" && "$quiet_page_body" == *"Quiet Page"* \
        && "$quiet_root_noise" == "0" \
        && "$quiet_long_code" == "$quiet_long_expected_code" \
        && "$quiet_long_log" == "$quiet_long_expected_log" \
        && "$quiet_long_noise" == "0" ]]; then
    report_result "Root request logs no path error" "$quiet_root_line" 0
  else
    report_result "Root request logs no path error" "$quiet_root_line" 1 \
      "Expected 200 + index body for '/', 200 + page body for /page, no 'Path too long' line, then 414 + exactly one line for an overlong URI. Got root:$quiet_root_code body:'$quiet_root_body' page:$quiet_page_code body:'$quiet_page_body' root_noise:$quiet_root_noise long:$quiet_long_code (want $quiet_long_expected_code) long_noise:$quiet_long_noise long_log:$quiet_long_log (want $quiet_long_expected_log)"
  fi
else
  skip_test "Root request logs no path error" "requires local binary access"
fi

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
  # A runtime error (not a compile error) must render the same diagnostics page.
  show_run_code=$(curl -s -o /dev/null -w "%{http_code}" \
    "http://$HOST:$SHOW_ERRORS_PORT/boom")
  show_run_body=$(curl -s "http://$HOST:$SHOW_ERRORS_PORT/boom")
  stop_matching_servers "$TARGET_EXEC -h $HOST -p $SHOW_ERRORS_PORT -l /tmp/tests-show-errors.log"
  if [[ "$show_code" == "500" && "$show_body" == *"Compilation Error"* \
        && "$show_body" != *"Oops! Something broke"* \
        && "$show_run_code" == "500" && "$show_run_body" == *"Runtime Error"* \
        && "$show_run_body" != *"Oops! Something broke"* ]]; then
    report_result "Show errors on errors      " "$show_errors_line" 0
  else
    report_result "Show errors on errors      " "$show_errors_line" 1 \
      "Expected 500 with 'Compilation Error' and 'Runtime Error', no generic page. Got compile:$show_code:'${show_body:0:80}' runtime:$show_run_code:'${show_run_body:0:80}'"
  fi
else
  skip_test "Show errors on compile error" "requires local binary access"
fi

if [[ "$TARGET_EXEC" != "-" ]]; then
  stop_matching_servers "$TARGET_EXEC -h $HOST -p $ECHO_PORT -l /tmp/tests-echo.log"
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
  stop_matching_servers "$TARGET_EXEC -h $HOST -p $CRON_PORT -l /tmp/tests-cron.log"
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

# Tests - Cron execution
# The reload test above proves the cron file is re-installed; it never proved a
# job runs. Cron.at(hour, minute, dayOfWeek) was broken on every tick: it read
# Date#weekday, which does not exist (the getter is dayOfWeek), so the 4-arg
# form raised "Date does not implement 'weekday'" instead of matching.
if [[ "$TARGET_EXEC" != "-" ]]; then
  cron_exec_line=$LINENO
  _test_start_ms=$(now_ms)
  cron_exec_root=$(mktemp -d)
  cron_exec_log=/tmp/tests-cron-exec.log
  rm -f "$cron_exec_log"
  cat > "$cron_exec_root/_cron.wren" <<'CRON_EOF'
var now = Date.now
Cron.every(1) { |d| System.print("CRON-EXEC-EVERY") }
Cron.at(now.hours, now.minutes, now.dayOfWeek) { |d| System.print("CRON-EXEC-AT") }
Cron.at((now.hours + 1) % 24, now.minutes) { |d| System.print("CRON-EXEC-MISS") }
CRON_EOF
  require_free_port "$HOST" "$CRON_PORT" "cron execution server"
  "$TARGET_EXEC" -h "$HOST" -p "$CRON_PORT" -l "$cron_exec_log" \
    "$cron_exec_root" > /dev/null 2>&1 &
  disown
  wait_port "$HOST" "$CRON_PORT" 10
  cron_exec_deadline=$(( $(now_ms) + 15000 ))
  while [[ $(now_ms) -lt $cron_exec_deadline ]]; do
    if grep -q "CRON-EXEC-EVERY" "$cron_exec_log" 2>/dev/null \
       && grep -q "CRON-EXEC-AT" "$cron_exec_log" 2>/dev/null; then
      break
    fi
    sleep 0.5
  done
  stop_matching_servers "$TARGET_EXEC -h $HOST -p $CRON_PORT -l $cron_exec_log"
  rm -rf "$cron_exec_root"
  cron_every_ran=0
  cron_at_ran=0
  cron_at_miss_ran=0
  grep -q "CRON-EXEC-EVERY" "$cron_exec_log" 2>/dev/null && cron_every_ran=1
  grep -q "CRON-EXEC-AT" "$cron_exec_log" 2>/dev/null && cron_at_ran=1
  grep -q "CRON-EXEC-MISS" "$cron_exec_log" 2>/dev/null && cron_at_miss_ran=1
  if [[ "$cron_every_ran" == 1 && "$cron_at_ran" == 1 && "$cron_at_miss_ran" == 0 ]]; then
    report_result "Cron jobs execute           " "$cron_exec_line" 0
  else
    report_result "Cron jobs execute           " "$cron_exec_line" 1 \
      "every:$cron_every_ran at:$cron_at_ran miss:$cron_at_miss_ran (log: $cron_exec_log)"
  fi
else
  skip_test "Cron jobs execute           " "requires local binary access"
fi

# Tests - bialet dev
# `bialet dev` must serve the current directory, turn on live reload and error
# pages for that run, inject the live-reload script into HTML, and bump the
# /_livereload version when a file in the app directory changes. It must not
# write the flags into the database: they used to stay on for every later plain
# `bialet` start, so production served error traces and /_livereload.
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
    "SELECT COUNT(*) FROM BIALET_CONFIG WHERE key IN ('BIALET_LIVE_RELOAD','BIALET_SHOW_ERRORS')" 2>/dev/null)
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
  stop_matching_servers "$dev_exec dev -q -h $HOST -p $DEV_PORT -l /tmp/tests-dev.log"
  # A plain start on the same database afterwards has neither feature.
  require_free_port "$HOST" "$DEV_PORT" "plain server after dev"
  (cd "$dev_root" && "$dev_exec" -q -h "$HOST" -p "$DEV_PORT" -l /tmp/tests-dev.log) \
    > /dev/null 2>&1 &
  disown
  wait_http "http://$HOST:$DEV_PORT/" 200 10
  dev_after=$(curl -s -o /dev/null -w "%{http_code}" "http://$HOST:$DEV_PORT/_livereload")
  stop_matching_servers "$dev_exec -q -h $HOST -p $DEV_PORT -l /tmp/tests-dev.log"
  if [[ "$dev_code" == "200" && "$dev_body" == *"Dev App"* && "$dev_live" == "1" \
        && "${dev_flags:-0}" == "0" && "$dev_after" == "403" \
        && "$dev_v1" != "$dev_v2" ]]; then
    report_result "bialet dev starts" "$dev_starts_line" 0
  else
    report_result "bialet dev starts" "$dev_starts_line" 1 \
      "Expected 200 + 'Dev App' + livereload script + no flags in the DB + 403 after a plain start + bumping version. Got code:$dev_code body:'$dev_body' livereload:$dev_live flags:$dev_flags after:$dev_after v1:$dev_v1 v2:$dev_v2"
  fi
  rm -f "$dev_root/_db.sqlite3"
else
  skip_test "bialet dev starts" "requires local binary access"
fi

# Tests - Port binding
# SO_REUSEADDR (needed so a restart is not blocked by sockets in TIME_WAIT)
# also lets a wildcard bind share a port with a listener bound to a specific
# address, and the other way round. The second server used to start "fine" and
# then serve nothing on 127.0.0.1: the kernel kept routing loopback to the
# older, more specific listener. Starting on a port that already answers must
# fail instead of being silently shadowed.
if [[ "$TARGET_EXEC" != "-" ]]; then
  bind_line=$LINENO
  _test_start_ms=$(now_ms)
  bind_port=$((PORT + 97))
  bind_root=$(mktemp -d)
  printf 'bind probe\n' > "$bind_root/index.html"
  require_free_port "127.0.0.1" "$bind_port" "port binding test"

  # Specific first, wildcard second: this is the reported case (`bialet -p N`
  # running, then `bialet -p N -h 0.0.0.0`).
  "$TARGET_EXEC" -h 127.0.0.1 -p "$bind_port" "$bind_root" > /dev/null 2>&1 &
  disown
  wait_port "127.0.0.1" "$bind_port" 10
  bind_out=$("$TARGET_EXEC" -h 0.0.0.0 -p "$bind_port" "$bind_root" 2>&1)
  bind_status=$?
  if [[ $bind_status -ne 0 && "$bind_out" == *"Could not bind port $bind_port"* ]]; then
    report_result "Wildcard bind over busy port " "$bind_line" 0
  else
    report_result "Wildcard bind over busy port " "$bind_line" 1 \
      "Expected a non-zero exit and 'Could not bind port $bind_port'. Got $bind_status: $bind_out"
  fi
  stop_matching_servers "$TARGET_EXEC -h 127.0.0.1 -p $bind_port"
  bind_deadline=$(( $(now_ms) + 5000 ))
  while check_port "127.0.0.1" "$bind_port" 0.2; do
    [[ $(now_ms) -lt $bind_deadline ]] || break
    sleep 0.1
  done

  # Wildcard first, specific second: the same shadowing running the other way.
  "$TARGET_EXEC" -h 0.0.0.0 -p "$bind_port" "$bind_root" > /dev/null 2>&1 &
  disown
  wait_port "127.0.0.1" "$bind_port" 10
  bind_out=$("$TARGET_EXEC" -h 127.0.0.1 -p "$bind_port" "$bind_root" 2>&1)
  bind_status=$?
  if [[ $bind_status -ne 0 && "$bind_out" == *"Could not bind port $bind_port"* ]]; then
    report_result "Specific bind over busy port " "$bind_line" 0
  else
    report_result "Specific bind over busy port " "$bind_line" 1 \
      "Expected a non-zero exit and 'Could not bind port $bind_port'. Got $bind_status: $bind_out"
  fi
  stop_matching_servers "$TARGET_EXEC -h 0.0.0.0 -p $bind_port"
  rm -rf "$bind_root"
else
  skip_test "Wildcard bind over busy port " "requires local binary access"
  skip_test "Specific bind over busy port " "requires local binary access"
fi

finish
print_summary >&2

exit $?
