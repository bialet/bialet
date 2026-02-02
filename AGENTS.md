# Bialet Development Guide

Bialet is a single-process C web framework embedding a heavily modified Wren
language and SQLite.

## Project Structure

- `src/` - C runtime & Wren glue
- `docs/` - User-facing documentation
- `docs/examples/` - App examples
- `tests/` - Integration tests

## Quick Commands

```bash
# Build
make

# Run tests
./tests/run.sh ./build/bialet 127.0.0.1 7111

# Alternative: make check (calls tests/run.sh)
make check

# Install (copies build/bialet to ~/.local/bin)
make install

# Clean build
make clean && make
```

## Runtime & Development Notes

### C Entrypoints

- `src/main.c` - Process lifecycle, reloads, cron, migrations
- `src/server.c` - HTTP dispatch, path resolution

### Special Wren Files

Wren app files are plain `.wren` files served from a root app directory:

- `_app.wren` - Site template / app namespace
- `_route.wren` - Per-directory route handlers (server searches upward), used
  only when SEO friendly URLs is a must. Prefer GET parameters otherwise.
- `_migration.wren` or `/_app/migration.wren` - DB migrations (run at start)
- `/_cron.wren` or `/_app/cron.wren` - Optional cron tasks
- `_db.sqlite3` - SQLite DB file for the app

### Security & Routing

Files starting with `_` or `.` are forbidden (server returns 403). The server
prefers `.wren` handlers, falls back to `.html`, and attempts directory index
files.

## Bialet App Patterns

- Think of Bialet apps as classic multi-page web apps, not single-page apps.
  Server renders full pages; use forms and links for navigation.
- Inline HTML: `{{ ... }}` interpolation inside `<...>` blocks
- Queries: Use backticks `` `SELECT ...` `` for prepared Query objects with `?`
  placeholders
- Request handling: `Request.isPost`, `Request.post(name)`, `Response.out(...)`,
  `Response.redirect(path)`
- DB values come back as strings - convert with `Num.fromString(...)` before
  numeric math or use `query.num` for direct numeric retrieval.

## Build & Link Notes

- `Makefile` uses `-lsqlite3`, `-lcurl`, and conditionally links OpenSSL
- Changes touching networking or DB need matching LDFLAGS/CFLAGS updates
- Wren sources embedded as C strings via `tools/wren_to_c_string.py` and
  `wren_*.wren.inc` files

## Tests & CI

- `tests/run.sh` drives integration checks by calling the built binary and
  asserting HTTP responses
- Documentation built with Sphinx (`docs/`); workflow in
  `.github/workflows/sphinx.yml`

## Debugging

- Use `System.print(...)` inside Wren files for runtime logs on server stdout
- Live-reload uses inotify on Linux to watch `.wren` files (see `src/main.c`)
- On macOS, restart the server manually for changes

## C Editing Guidelines

- Preserve public APIs in `src/*.h`
- Avoid reordering exported struct/layout changes without adjusting consumers
- Prefer small, focused changes
- Run `make` then `make check` to validate
