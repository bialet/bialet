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
- [ ] **Document Tailwind CLI integration** — Step-by-step guide for
      self-hosting Tailwind with `tailwindcss --watch` against Bialet app
      directory (dev + production workflows)
- [ ] **JSX-to-Bialet migration guide** — Cheatsheet covering: tag nesting
      limitations, map callback restrictions, interpolation rules,
      component-as-method pattern, and raw HTML security footguns

## Longer Term / Ideas

- [ ] **Admin dashboard** — External lib UI for browsing the database and
      viewing logs
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

## How to Contribute

See [CONTRIBUTING.md](CONTRIBUTING.md) for setup and guidelines.

Feature requests and feedback are welcome. Open an issue with the "enhancement"
label on [GitHub Issues](https://github.com/bialet/bialet/issues).
