# Roadmap

Bialet is stable and feature-complete for my personal use. This roadmap tracks
ideas I may revisit if interest returns, but no promises.

## Editors/IDE Support

- [ ] VS Code (syntax JSON, language-configuration)
- [ ] Sublime Text (.sublime-syntax and .tmPreferences)
- [x] Vim / Neovim (ftdetect, syntax)
- [ ] JetBrains (IntelliJ) (TextMate bundle or custom plugin)

## On my radar

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
- [ ] **HTTP Client - Response cookies / cookie jar: per-host scoping** — the
      jar is process-wide and sends cookies regardless of domain; scope by host
      and honor `Domain`/`Path`/`Secure` attributes.
- [ ] **Query params silently drop non-primitive values** — `queryPrepare`
      only binds `null`/`bool`/`num`/`string` params; any other value (e.g. an
      `HtmlNode`) is skipped, shifting every later column left and silently
      corrupting the row. Stringify non-primitive params (via `toString`) or
      error loudly instead of dropping them.

## Someday maybe

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
