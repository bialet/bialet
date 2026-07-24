# Roadmap

Bialet is in active development. This roadmap tracks planned features and
improvements.

## Near Term

- [ ] **Editor syntax highlighting** — Add support for major editors (VS Code,
      Sublime, etc.)
- [ ] **AI development skills** — Create skills/prompts for AI-assisted Bialet
      development
- [ ] **Documentation review** — Audit and fill gaps in existing docs, ensure
      all features are properly covered (e.g. custom errors)
- [ ] **Wren programming guide** — Document how to write and integrate Wren
      scripts in Bialet apps
- [ ] **Production deployment guide** — Document how to deploy on a VPS with
      systemd, reverse proxy setup, and environment configuration
- [ ] **Documentation improvements** — General cleanup, better structure, and
      more examples
- [ ] **i18n / localization** — Translation helpers and locale-aware formatting
- [ ] **Admin dashboard** — Built-in UI for browsing the database and viewing
      logs
- [ ] **Data filtering lib** — Query sanitization and type-safe filtering
      utilities

## Longer Term

- [ ] **Live-reload** — Watch files and auto-refresh browser via WebSocket on
      changes
- [ ] **LSP support** — Language Server Protocol implementation with
      autocomplete, go-to-definition, and diagnostics for VS Code
- [ ] **HTTPS / TLS support** — Native TLS in the server binary (no reverse
      proxy needed for basic deployments)
- [ ] **MySQL and PostgreSQL support** — Optional alternative database backend
- [ ] **WebAssembly target** — Compile Bialet apps to Wasm for edge deployment
- [ ] **Opcode Cache** — Compile scripts to cached bytecode for faster request
      handling

## How to Contribute

See [CONTRIBUTING.md](CONTRIBUTING.md) for setup and guidelines.

Feature requests and feedback are welcome. Open an issue with the "enhancement"
label on [GitHub Issues](https://github.com/bialet/bialet/issues).
