# Roadmap

Bialet is in active development. This roadmap tracks planned features and
improvements.

## Editors/IDE Support

- [ ] VS Code (syntax JSON, language-configuration)
- [ ] Sublime Text (.sublime-syntax and .tmPreferences)
- [x] Vim / Neovim (ftdetect, syntax)
- [ ] JetBrains (IntelliJ) (TextMate bundle or custom plugin)

## Near Term

### 0.11.0

- [x] **AI development skills** — Create skills/prompts for AI-assisted Bialet
      development
- [x] **Documentation review** — Audit and fill gaps in existing docs, ensure
      all features are properly covered (e.g. custom errors)
- [x] **Wren programming guide** — Document how to write and integrate Wren
      scripts in Bialet apps
- [x] **Production deployment guide** — Document how to deploy on a VPS with
      systemd, reverse proxy setup, and environment configuration
- [ ] **Data filtering lib** — Query sanitization and type-safe filtering
      utilities
- [ ] **Forms guide** — Document POST handling, basic validation, redirect after
      POST (PRG), and simple file uploads.
- [x] **Security guide** — Central page covering XSS prevention, SQL injection
      reminders, secure configuration, and any built-in protections (e.g.,
      CSRF).
- [ ] **Configuration guide** — Reading/writing `BIALET_CONFIG`,
      environment-specific settings, and server options (port, host).
- [ ] **Reorganise documentation into groups** — Reduce sidebar length by
      grouping related pages:
  - Getting started (Why Bialet?, Tutorial, Installation)
  - Core concepts (Wren, Routing, Templates, Database)
  - Web development (Forms, REST APIs, File handling, Date/time)
  - Operations (Security, Configuration, Cron, Deployment)
  - Reference (API Reference, Examples, FAQ)

### 0.12.0

- [ ] **Error handling & logging** — Custom 404/500 pages, `System.print` usage,
      and debugging best practices.
- [ ] **Admin dashboard** — Built-in UI for browsing the database and viewing
      logs

### 0.13.0

- [ ] **Live-reload** — Watch files and auto-refresh browser via WebSocket on
      changes

## Longer Term / Ideas

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

## How to Contribute

See [CONTRIBUTING.md](CONTRIBUTING.md) for setup and guidelines.

Feature requests and feedback are welcome. Open an issue with the "enhancement"
label on [GitHub Issues](https://github.com/bialet/bialet/issues).
