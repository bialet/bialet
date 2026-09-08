# Roadmap

Bialet is stable for personal use. Items under *Priority* are real
dependencies of deployments that run on it and take priority. The rest are
ideas I may revisit if interest returns, but no promises.

## Priority

- [x] **Expose upload and SQLite limits as startup config** — `-u /
      --max-upload-size` (bytes or K/M/G suffix; default 4 MB), `-f /
      --foreign-keys` (on/off), and `-s / --synchronous` (off/normal/full/
      extra). The upload cap is clamped at startup to what the request-body
      cap (`-b` / `-m`) can actually deliver, so rejection logs state a
      reachable limit; docs updated to match (default effective upload is ~120
      KB until `-b` and `-m` are raised).
- [ ] **`bialet check` — whole-tree validation** — `-t` validates a single
      `.wren` file. A deploy hook needs a command that validates every `.wren`
      in an app tree and exits `0`/`1`. The docs and manifesto advertise
      `init`/`check`/`test`, but only `validate`/`tests`/`dev` exist.
- [ ] **Hidden health endpoint** — a reserved `_`-prefixed route (e.g.
      `/_health`) returning version, uptime, and DB status so supervisors and
      watchdogs can health-check an app without relying on an app route.

## Editors/IDE Support

- [x] VS Code (syntax JSON, language-configuration)
- [ ] Sublime Text (.sublime-syntax and .tmPreferences)
- [x] Vim / Neovim (ftdetect, syntax)
- [ ] JetBrains (IntelliJ) (TextMate bundle or custom plugin)

## On my radar

- [ ] **Per-request access logs** — `-l` should record one line per request
      (method, path, status, duration, bytes) so a host can meter usage and
      detect abuse without parsing proxy logs.
- [ ] **Optional on-disk file storage** — a flag to store uploads on disk
      (under the app's data dir, outside the code release) instead of only as
      DB blobs. Large files currently inflate the database, backups, and the
      disk quota all at once.
- [ ] **`bialet init`** — scaffold a new app directory, completing the
      `init`/`check`/`test` story the docs promise.
- [ ] **Admin dashboard** — External lib UI for browsing the database and
      viewing logs
- [ ] **Data filtering lib** — External lib for query sanitization and type-safe
      filtering utilities
- [ ] **i18n / localization** — Translation helpers and locale-aware formatting
- [ ] **LSP support** — Language Server Protocol implementation with
      autocomplete, go-to-definition, and diagnostics for VS Code
- [ ] **Opcode Cache** — Compile scripts to cached bytecode for faster request
      handling
- [ ] **HTTP Client - Multipart / file uploads** — no way to send files or
      `multipart/form-data` to an external API.
- [x] **HTTP Client - Response cookies / cookie jar: per-host scoping** — the
      jar is process-wide and sends cookies regardless of domain; scope by host
      and honor `Domain`/`Path`/`Secure` attributes.
- [x] **Query params silently drop non-primitive values** — `queryPrepare`
      only binds `null`/`bool`/`num`/`string` params; any other value (e.g. an
      `HtmlNode`) is skipped, shifting every later column left and silently
      corrupting the row. Stringify non-primitive params (via `toString`) or
      error loudly instead of dropping them.

## Someday maybe

- [ ] **Official "hosted" deployment recipe** — a doc page showing the
      recommended hosted pattern: immutable release root + external DB via
      `-d` + `umask 0077`. `deployment.md` covers the pieces; this would be
      the full recipe.
- [ ] **Warn when a block callback returns null** — a multi-statement `map`
      callback (or any block whose body is not a single expression) renders
      empty output with no error. Log a warning so a silent empty `<ul>` is
      distinguishable from an empty query result.
- [ ] **WebAssembly target** — Compile Bialet apps to Wasm for edge deployment
- [ ] **MySQL and PostgreSQL support** — Optional alternative database backend
- [ ] **HTTPS / TLS support** — Native TLS in the server binary (no reverse
      proxy needed for basic deployments)

## How to Contribute

See [CONTRIBUTING.md](CONTRIBUTING.md) for setup and guidelines.

Feature requests and feedback are welcome. Open an issue with the "enhancement"
label on [GitHub Issues](https://github.com/bialet/bialet/issues).
