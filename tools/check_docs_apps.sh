#!/bin/bash
#
# Walks the self-contained example apps in docs/examples over HTTP.
#
# `make check-examples` validates every snippet with `bialet -t` and
# `bialet -T docs/examples` runs the tests that live in that directory. Neither
# can exercise an app that owns its schema and its `_app.wren`: those only work
# when the folder itself is the app root. guestbook, guestbook-styled and
# inventory are exactly that, and until this script existed nothing ran them.
#
# Each app is copied to a temporary directory (so its migrations start from an
# empty database and the source tree is never written to), booted on a free
# port, and driven through its main flow with curl. The inventory flow covers
# login, CSRF, a multipart create, a search, a photo fetch, delete and logout.
#
# Usage: tools/check_docs_apps.sh [executable] [host]
#
# Exits 0 when every assertion passes (or when curl is unavailable, in which
# case it says so and skips), 1 otherwise.

set -uo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BINARY="${1:-$ROOT_DIR/build/bialet}"
HOST="${2:-127.0.0.1}"
EXAMPLES="$ROOT_DIR/docs/examples"

if ! command -v curl >/dev/null 2>&1; then
  echo "check_docs_apps: curl not found, skipping example app walkthroughs"
  exit 0
fi
if [[ ! -x "$BINARY" ]]; then
  echo "check_docs_apps: $BINARY is not executable (run make first)" >&2
  exit 1
fi
# The app runs from its own temporary directory, so the binary must be absolute.
BINARY="$(cd "$(dirname "$BINARY")" && pwd)/$(basename "$BINARY")"

PASS=0
FAIL=0
FAILED_NAMES=()
SERVER_PID=""
APP_DIR=""

cleanup() {
  if [[ -n "$SERVER_PID" ]]; then
    kill "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null
  fi
  if [[ -n "$APP_DIR" ]]; then
    rm -rf "$APP_DIR"
  fi
}
trap cleanup EXIT

free_port() {
  python3 - <<'PY'
import socket
s = socket.socket()
s.bind(("127.0.0.1", 0))
print(s.getsockname()[1])
s.close()
PY
}

# Starts a copy of docs/examples/<name> as the app root and waits for it.
start_app() {
  local name="$1"
  PORT="$(free_port)"
  APP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/bialet-docs-app.XXXXXX")"
  cp -R "$EXAMPLES/$name/." "$APP_DIR/"
  # The served root holds a SQLite file per app; keep test noise out of stdout.
  ( cd "$APP_DIR" && "$BINARY" -h "$HOST" -p "$PORT" -q . >"$APP_DIR/server.log" 2>&1 ) &
  SERVER_PID=$!
  local deadline=$(( $(date +%s) + 20 ))
  while (( $(date +%s) < deadline )); do
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
      echo "check_docs_apps: $name exited during startup" >&2
      cat "$APP_DIR/server.log" >&2 2>/dev/null
      return 1
    fi
    if curl -s -o /dev/null --max-time 1 "http://$HOST:$PORT/"; then
      return 0
    fi
    sleep 0.1
  done
  echo "check_docs_apps: $name did not start on port $PORT" >&2
  cat "$APP_DIR/server.log" >&2 2>/dev/null
  return 1
}

stop_app() {
  if [[ -n "$SERVER_PID" ]]; then
    kill "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null
    SERVER_PID=""
  fi
  if [[ -n "$APP_DIR" ]]; then
    rm -rf "$APP_DIR"
    APP_DIR=""
  fi
}

report() {
  local label="$1" ok="$2" detail="${3:-}"
  if [[ "$ok" == "1" ]]; then
    printf '  \033[0;32m✓\033[0m %s\n' "$label"
    PASS=$((PASS + 1))
  else
    printf '  \033[0;31m✗\033[0m %s%s\n' "$label" "${detail:+ ($detail)}"
    FAIL=$((FAIL + 1))
    FAILED_NAMES+=("$label")
  fi
}

# get_code <path> [curl args...] -> prints the status code, body in $BODY
request() {
  local method="$1" path="$2" data="${3:-}" jar="${4:-}"
  BODY="$(mktemp "${TMPDIR:-/tmp}/bialet-docs-body.XXXXXX")"
  local args=(-s -o "$BODY" -w '%{http_code}' -X "$method"
              --max-time 20 "http://$HOST:$PORT$path")
  [[ -n "$jar" ]] && args+=(-b "$jar" -c "$jar")
  [[ -n "$data" ]] && args+=(-d "$data")
  CODE="$(curl "${args[@]}")"
}

expect_code() { # label method path expected [data] [jar]
  local label="$1" method="$2" path="$3" expected="$4" data="${5:-}" jar="${6:-}"
  request "$method" "$path" "$data" "$jar"
  report "$label" "$([[ "$CODE" == "$expected" ]] && echo 1 || echo 0)" \
         "want $expected, got $CODE"
}

expect_contains() { # label method path needle [data] [jar]
  local label="$1" method="$2" path="$3" needle="$4" data="${5:-}" jar="${6:-}"
  request "$method" "$path" "$data" "$jar"
  if [[ "$CODE" != "200" ]]; then
    report "$label" 0 "want 200, got $CODE"
    return
  fi
  if grep -Fq -- "$needle" "$BODY"; then
    report "$label" 1
  else
    report "$label" 0 "missing \"$needle\""
  fi
}

expect_missing() { # label method path needle [data] [jar]
  local label="$1" method="$2" path="$3" needle="$4" data="${5:-}" jar="${6:-}"
  request "$method" "$path" "$data" "$jar"
  if grep -Fq -- "$needle" "$BODY"; then
    report "$label" 0 "\"$needle\" was present"
  else
    report "$label" 1
  fi
}

csrf_token() { # jar
  request GET /login "" "$1"
  grep -o 'name="_bialet_csrf" value="[^"]*"' "$BODY" \
    | head -1 \
    | sed 's/.*value="//;s/"$//'
}

# ---------------------------------------------------------------- guestbook
if start_app guestbook; then
  expect_contains "guestbook: index renders       " GET / "Bialet Guestbook"
  expect_contains "guestbook: empty-state list    " GET / "<ul>"
  expect_contains "guestbook: message saved       " POST /message "hello from the docs test" \
                  "msg=hello%20from%20the%20docs%20test"
  expect_contains "guestbook: message persisted   " GET / "hello from the docs test"
  expect_missing  "guestbook: markup is escaped   " POST /message "<script>x</script>" \
                  "msg=%3Cscript%3Ex%3C%2Fscript%3E"
  expect_missing  "guestbook: no raw injection    " GET / "<script>x</script>"
  expect_code     "guestbook: empty POST redirects" POST /message 302 "" ""
  stop_app
else
  report "guestbook: starts              " 0
fi

# --------------------------------------------------------- guestbook-styled
if start_app guestbook-styled; then
  expect_contains "styled guestbook: layout       " GET / "Say something nice"
  expect_contains "styled guestbook: stylesheet   " GET / 'href="/css/style.css"'
  expect_code     "styled guestbook: stylesheet 200" GET /css/style.css 200
  expect_contains "styled guestbook: saved page   " POST /message "Saved <b>styled message</b>" \
                  "msg=styled%20message"
  expect_contains "styled guestbook: shows entry  " GET / "styled message"
  expect_code     "styled guestbook: redirect     " GET /?new=1 200
  stop_app
else
  report "styled guestbook: starts       " 0
fi

# ---------------------------------------------------------------- inventory
if start_app inventory; then
  JAR="$(mktemp "${TMPDIR:-/tmp}/bialet-docs-jar.XXXXXX")"
  PHOTO="$(mktemp "${TMPDIR:-/tmp}/bialet-docs-photo.XXXXXX")"
  printf '\x89PNG\r\n\x1a\nfake-photo-bytes' > "$PHOTO"

  expect_code     "inventory: anonymous gate      " GET / 302 "" ""
  expect_code     "inventory: login page          " GET /login 200 "" "$JAR"
  TOKEN="$(csrf_token "$JAR")"
  report "inventory: login sets CSRF token" "$([[ -n "$TOKEN" ]] && echo 1 || echo 0)"

  request POST /login "email=admin%40example.com&password=changeme&_bialet_csrf=$TOKEN" "$JAR"
  report "inventory: login redirects      " "$([[ "$CODE" == "302" ]] && echo 1 || echo 0)" \
         "want 302, got $CODE"
  expect_contains "inventory: empty stock list    " GET /items "No items" "" "$JAR"

  request GET /item "" "$JAR"
  ITEM_TOKEN="$(grep -o 'name="_bialet_csrf" value="[^"]*"' "$BODY" \
    | head -1 | sed 's/.*value="//;s/"$//')"
  report "inventory: create form CSRF    " "$([[ -n "$ITEM_TOKEN" ]] && echo 1 || echo 0)"

  # multipart/form-data: name, sku, quantity, notes and a photo file
  curl -s -o /dev/null -w '%{http_code}' --max-time 20 \
    -b "$JAR" -c "$JAR" \
    -F "_bialet_csrf=$ITEM_TOKEN" \
    -F "name=Widget" -F "sku=SKU-1" -F "quantity=7" -F "notes=Test notes" \
    -F "photo=@$PHOTO;type=image/png" \
    "http://$HOST:$PORT/item" > "$BODY"
  CODE="$(cat "$BODY")"
  report "inventory: multipart create    " "$([[ "$CODE" == "302" ]] && echo 1 || echo 0)" \
         "want 302, got $CODE"

  expect_contains "inventory: item listed         " GET /items "Widget" "" "$JAR"
  expect_contains "inventory: item detail         " GET /items/1 "SKU-1" "" "$JAR"
  expect_contains "inventory: notes rendered      " GET /items/1 "Test notes" "" "$JAR"
  expect_code     "inventory: photo served        " GET /photo?id=1 200 "" "$JAR"
  expect_contains "inventory: search matches      " GET "/items?q=Widget" "Widget" "" "$JAR"
  expect_missing  "inventory: search filters      " GET "/items?q=Nomatch" "Widget" "" "$JAR"
  expect_code     "inventory: unknown photo 404   " GET /photo?id=999 404 "" "$JAR"
  expect_code     "inventory: unknown item 404    " GET /items/999 404 "" "$JAR"

  # Delete needs the CSRF token from the detail page.
  request GET /items/1 "" "$JAR"
  DEL_TOKEN="$(grep -o 'name="_bialet_csrf" value="[^"]*"' "$BODY" \
    | head -1 | sed 's/.*value="//;s/"$//')"
  expect_code     "inventory: delete redirects    " POST /item-delete 302 \
                  "id=1&_bialet_csrf=$DEL_TOKEN" "$JAR"
  expect_contains "inventory: item removed        " GET /items "No items" "" "$JAR"

  request GET /items "" "$JAR"
  OUT_TOKEN="$(grep -o 'name="_bialet_csrf" value="[^"]*"' "$BODY" \
    | head -1 | sed 's/.*value="//;s/"$//')"
  expect_code     "inventory: logout redirects    " POST /logout 302 \
                  "_bialet_csrf=$OUT_TOKEN" "$JAR"
  expect_code     "inventory: session destroyed   " GET /items 302 "" "$JAR"

  rm -f "$JAR" "$PHOTO"
  stop_app
else
  report "inventory: starts              " 0
fi

# --------------------------------------------------------------------- summary
echo
if (( FAIL == 0 )); then
  printf '\033[0;32mcheck_docs_apps: %d assertions passed\033[0m\n' "$PASS"
  exit 0
fi
printf '\033[0;31mcheck_docs_apps: %d passed, %d failed\033[0m\n' "$PASS" "$FAIL"
for name in "${FAILED_NAMES[@]}"; do
  printf '  failed: %s\n' "$name"
done
exit 1
