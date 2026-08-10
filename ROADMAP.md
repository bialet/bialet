# Roadmap

Bialet is in active development. This roadmap tracks planned features and
improvements.

## Editors/IDE Support

- [ ] VS Code (syntax JSON, language-configuration)
- [ ] Sublime Text (.sublime-syntax and .tmPreferences)
- [x] Vim / Neovim (ftdetect, syntax)
- [ ] JetBrains (IntelliJ) (TextMate bundle or custom plugin)

## Near Term

- [ ] **Auto HTML escaping** — `{{ value }}` should be escaped by default
- [x] **Document Tailwind CLI integration** — Step-by-step guide for
      self-hosting Tailwind with `tailwindcss --watch` against Bialet app
      directory (dev + production workflows)
- [ ] **JSX-to-Bialet migration guide** — Cheatsheet covering: tag nesting
      limitations, map callback restrictions, interpolation rules,
      component-as-method pattern, and raw HTML security footguns
- [x] **Live reload: fix the `/_livereload` version bump** — the injected
      polling script never reloaded the browser because the version number
      did not change when files in the app directory were created, modified,
      or deleted. On Linux the HTTP child process served a copy of the
      version frozen at fork time while the parent's file-watch thread
      updated its own, and in `bialet dev` the server loop reaped the
      browser child instead of the HTTP child, tearing down the watcher.
      The version counter now lives in a shared mapping and the server waits
      for its HTTP child specifically.
- [ ] **Docs: state that mismatched closing tags are accepted** —
      `template.md` currently claims `<div><span>Hello</div>` fails to
      compile; the parser accepts it and serves the HTML verbatim. Document
      that mismatched tags are not validated.
- [ ] **Docs: more Wren method-body examples** — add explicit examples of
      single-line vs multi-line method bodies: a body is an expression body
      (implicit return) only when the expression starts on the same line as
      the `{`; a statement body on a following line returns `null` silently.
- [ ] **Docs: start beginners with `bialet dev`** — the getting-started flow
      should run `bialet dev` from the first page so the in-browser error
      display and live reload are on by default instead of a bare `bialet`
      with a generic error page.
- [ ] **Docs: note config is read at startup** — `BIALET_SHOW_ERRORS` and
      `BIALET_LIVE_RELOAD` are read when the server starts; enabling them
      while a server is running requires a restart. Document this.
- [ ] **CLI: reject unsupported long-form flags** — `bialet --version`
      silently starts a server on port 7001 instead of printing the version.
      Reject unknown long-form options rather than falling through to
      "serve the current directory".
- [ ] **Docs: fix stale claims vs the current binary** — accept `<br/>`
      (no space) as valid (`template.md` calls it "Incorrect"), drop the
      "double-response error" claim for a forgotten `return` before
      `Response.redirect`, correct the session table name in `database.md`
      (`BIALET_SESSIONS` → `BIALET_SESSION`), qualify `wren.md`'s "null is
      safe", and fix the `security.md` intro that contradicts auto-escaping.
- [ ] **Compiler: friendlier invalid-tag-name errors** — `<MyElement>` gives
      `Error at '<': Expected expression.` and `<my_component>` gives
      `Unterminated HTML string.`; emit `Invalid tag name: must be lowercase
      alphanumeric + hyphens` instead.

## Longer Term / Ideas

- [ ] **Admin dashboard** — External lib UI for browsing the database and
      viewing logs
- [ ] **Warn when a block callback returns null** — a multi-statement `map`
      callback (or any block whose body is not a single expression) renders
      empty output with no error. Log a warning so a silent empty `<ul>`
      is distinguishable from an empty query result.
- [ ] **Data filtering lib** — External lib for query sanitization and type-safe
      filtering utilities
- [ ] **Opcode Cache** — Compile scripts to cached bytecode for faster request
      handling
- [ ] **Static assets** — Serving CSS, JS, and images; MIME types and cache
      headers (could be a small section or appendix).
- [ ] **i18n / localization** — Translation helpers and locale-aware formatting
- [ ] **LSP support** — Language Server Protocol implementation with
      autocomplete, go-to-definition, and diagnostics for VS Code
- [ ] **WebAssembly target** — Compile Bialet apps to Wasm for edge deployment
- [ ] **MySQL and PostgreSQL support** — Optional alternative database backend
- [ ] **HTTPS / TLS support** — Native TLS in the server binary (no reverse
      proxy needed for basic deployments)

## HTTP Client

Outbound HTTP (`Http` class, `src/http_call.c`). The current client covers
GET/POST/PUT/DELETE, custom headers, Basic auth, bearer tokens, form bodies,
a persistent cookie jar, per-call timeouts, redirects, and JSON handling.

- [x] **Per-call timeout options** — `options["timeout"]` / `options["connectTimeout"]`
      (milliseconds) override the 20s / 2s defaults in `http_call_perform`.
- [x] **Form-encoded bodies** — `options["form"]` map is URL-encoded and sent as
      `application/x-www-form-urlencoded`.
- [x] **Response cookies / cookie jar** — `Set-Cookie` headers are stored in a
      process-wide jar and sent back on subsequent calls unless the caller
      provides its own `Cookie` header.
- [x] **Bearer token / auth shortcut** — `options["token"]` sends
      `Authorization: Bearer <token>`.
- [x] **Query-string builder** — `Http.url(base, params)` appends URL-encoded
      query parameters to a URL (`Http.query(params)` returns the encoded
      string alone).
- [x] **Expose the curl error string** — `Http.error` remains the numeric code;
      `Http.errorMessage` now carries the underlying curl message.
- [ ] **Multipart / file uploads** — no way to send files or
      `multipart/form-data` to an external API.
- [ ] **Response cookies / cookie jar: per-host scoping** — the jar is
      process-wide and sends cookies regardless of domain; scope by host and
      honor `Domain`/`Path`/`Secure` attributes.
- [x] **`strtok()` in `http_call_perform`** — uses the process-global tokenizer;
      switch to `strtok_r` (see Security Hardening Backlog).

## Security Hardening Backlog

- [ ] **Compiler: bound HTML tag-name buffer** — `readHtmlString` in
      `src/wren_compiler.c` writes tag names into a fixed 64-byte buffer with no
      bounds check; an overlong tag name overflows the heap during compile.
      Bound `i` against `MAX_METHOD_NAME` (or grow with a `ByteBuffer`) and cap
      `numHandlebars` against `MAX_INTERPOLATION_NESTING`.
- [ ] **`Markdown.file`: close check-to-open TOCTOU** — `bialet_read_file` in
      `src/bialet_wren.c` validates with `realpath()` then re-opens with plain
      `fopen`, which follows symlinks. Reuse the `open_no_follow` path used
      elsewhere.
- [ ] **Serialize `bialet_run` on the shared SQLite handle** — the cron thread
      and the dmon file-watch thread both run Wren against the global `sqlite3*`
      connection without a shared lock. Widen the cron mutex to cover migrations
      and file-watch-triggered runs.
- [ ] **Windows: no-follow file open** — `open_file_within_root` degrades to
      bare `fopen` on Windows, following junctions; the POSIX `O_NOFOLLOW` walk
      has no Windows equivalent.
- [ ] **Windows: secure test-mode temp DB** — the `-T` path uses a predictable
      PID-based `/tmp/bialet_test_<pid>.sqlite3`; use `mkstemp`/O_EXCL semantics
      on Windows too.
- [ ] **Windows: fix realpath shim buffer size** — the Windows `realpath` shim
      writes up to `_MAX_PATH` (260) bytes into a 100-byte stack buffer; pass
      the caller's real size through to `_fullpath`/`WideCharToMultiByte`.
- [x] **`_route.wren` symlink bypasses the private-file rule** — the
      `_route.wren` directory search uses `stat()` (follows symlinks) and sets
      `private_path_internal`, which skips the resolved-path `_`/`.` check in
      `handle_client` (`src/server.c`). A planted `sub/_route.wren -> ../_db.sqlite3`
      makes the server serve the database. Only waive the private check when
      the resolved basename is exactly `_route.wren`, and use `lstat`/no-follow
      in the search.
- [x] **`setTimezone` uses `putenv()` with a stack buffer** — `setTimezone` in
      `src/wren_core.c` builds `TZ=<tz>` in a 64-byte stack buffer and passes it
      to `putenv()`, which stores the pointer; the second call invalid-frees the
      dead stack address (abort on glibc) or leaves a dangling `TZ` entry. Use
      `setenv("TZ", tz, 1)` / `unsetenv("TZ")` or a process-lifetime buffer.
- [ ] **`fork()` while cron/dmon threads are inside SQLite/Wren** — the Linux
      parent forks the HTTP child while the cron and dmon threads may hold
      SQLite/Wren locks (`src/main.c`); the child can deadlock or see torn heap.
      Register `pthread_atfork` handlers.
- [x] **`strtok()` races across cron/dmon/HTTP threads** — process-global
      tokenizer state in `http_call_perform` (`src/http_call.c`), the remote
      module loader (`src/bialet_wren.c`), and markdown table rendering
      (`src/markdown.c`) interleaves across concurrent `bialet_run` contexts.
      Replace with `strtok_r`/local state.
- [ ] **Session IDs and CSRF tokens from `sqlite3_randomness`** — the session/
      CSRF generator uses SQLite's documented non-cryptographic PRNG
      (`src/wren_core.c`). Use the same CSPRNG already used for password salts.
- [ ] **Multipart uploads have no steady-state purge** — `save_uploaded_files`
      grows `BIALET_FILES` ~10 MB per request with no disk recovery; cap
      aggregate storage or purge old blobs.
- [ ] **Outbound HTTP client has no response-size cap** — `write_callback` in
      `src/http_call.c` reallocs without bound; set `CURLOPT_MAXFILESIZE` or an
      equivalent.
- [ ] **Unchecked `fread` in `custom_error`** — an empty `{status}.html` yields
      a body without a trailing NUL and the `strlen` fallback over-reads the
      heap (`src/server.c`). NUL-terminate or track the length.
- [ ] **Unchecked `sscanf` leaves `HttpResponse.status` uninitialized** — on
      Windows, a parse failure leaves `status` uninitialized and it is consumed
      by the remote-module loader (`src/http_call.c`).
- [ ] **`test_runRequest` unbounded stack copies** — `strncpy` of the method and
      URI into 32-byte/1024-byte stack buffers with no size guard
      (`src/wren_core.c`); reachable in `-T` test mode via hostile `_tests/*.wren`.
- [ ] **Windows: no-follow open for Wren file/module reads** — extend the
      no-follow item to `read_file` (`src/bialet_wren.c`), which still uses
      plain `fopen` on Windows and follows junctions.

## How to Contribute

See [CONTRIBUTING.md](CONTRIBUTING.md) for setup and guidelines.

Feature requests and feedback are welcome. Open an issue with the "enhancement"
label on [GitHub Issues](https://github.com/bialet/bialet/issues).
