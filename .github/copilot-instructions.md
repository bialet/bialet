# Guidance for AI coding agents working on Bialet

Keep this short and actionable — specific to this repository.

- Purpose: Bialet is a single-process C web framework embedding the Wren language and SQLite. Key places: `src/` (C runtime & Wren glue), `docs/` (user-facing examples), `examples/` (app examples), and `tests/` (integration tests).

- Quick commands (from repo):
  - Build: `make clean && make`
  - Run tests: `make check` (calls `tests/run.sh`)
  - Install: `make install` (copies `build/bialet` to `~/.local/bin`)
  - Docker build/run: see the `Dockerfile` (image uses `ENTRYPOINT ["bialet", "-h", "0.0.0.0", "/var/www/"]`).

- Runtime & development notes:
  - The C entrypoints are `src/main.c` (process lifecycle, reloads, cron, migrations) and `src/server.c` (HTTP dispatch, path resolution).
  - Wren app files are plain `.wren` files served from a root app directory. Special filenames:
    - `_app.wren` — site template / app namespace
    - `_route.wren` — per-directory route handlers (server searches upward for this)
    - `_migration.wren` or `/_migration.wren` — DB migrations (run at start)
    - `/_cron.wren` — optional cron tasks
    - `_db.sqlite3` — the SQLite DB file for the app
  - Security / routing: files starting with `_` or `.` are forbidden (server returns 403). The server prefers `.wren` handlers, falls back to `.html`, and will attempt directory index files.

- Wren app patterns worth citing (examples in `docs/getting-started.md` and `examples/`):
  - Inline HTML strings use `{{ ... }}` interpolation inside `<...>` blocks.
  - Queries use backticks: `` `SELECT ...` `` producing prepared Query objects — use `?` placeholders and `.query(...)` or `.fetch()`.
  - Use `Request.isPost`, `Request.post(name)`, `Response.out(...)`, and `Response.redirect(path)` as shown in the docs.
  - DB values come back as strings — convert with `Num.fromString(...)` before numeric math.

- Build/link specifics to watch for in edits:
  - `Makefile` uses `-lsqlite3`, `-lcurl`, and conditionally links OpenSSL. Changes touching networking or DB need matching LDFLAGS/CFLAGS updates.
  - Wren sources are sometimes embedded as C strings via `tools/wren_to_c_string.py` and `wren_*.wren.inc` files — be careful when moving/renaming `.wren` files used by the runtime.

- Tests & CI:
  - `tests/run.sh` drives integration checks by calling the built binary (or `./build/bialet`) and asserting HTTP responses. Modify tests with care — they simulate end-to-end behavior.
  - Documentation is built with Sphinx (`docs/`); workflow exists in `.github/workflows/sphinx.yml`.

- Quick debugging tips:
  - Use `System.print(...)` inside Wren files to emit runtime logs seen on the server stdout.
  - For live-reload behavior, the binary uses inotify on Linux to watch `.wren` files (see `src/main.c`). On macOS you may need to restart the server.

- When editing C:
  - Preserve public APIs in `src/*.h` and avoid reordering exported struct/layout changes without adjusting consumers.
  - Prefer small, focused changes. Run `make` then `make check` to validate.

If anything above is unclear or you want examples inserted for a specific task (e.g., adding a route, writing a migration, or extending the C runtime), tell me which area and I'll expand this file with exact code snippets and file references.
