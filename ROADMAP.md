# Roadmap

Bialet is in active development. This roadmap tracks planned features and
improvements. Priorities may shift based on community feedback.

## Current Status — v0.10

- File-based routing with `.wren` and `.html`
- Inline HTML strings with interpolation (`{{ }}`)
- Built-in SQLite with migrations, Query objects, `.fetch()`, `.first()`,
  `.val()`, `.toNum()`, `.toBool()`
- CRUD operations via `Db.save()` and `Db.delete()`
- Sessions, cookies, CSRF protection
- CORS support for REST APIs
- Cron-style scheduled tasks (`Cron.every`, `Cron.at`)
- File upload and serving from database
- JSON parsing/stringifying, Base64, URL encoding
- HTTP client (GET, POST, PUT, DELETE) with libcurl
- Password hashing (`Util.hash`, `Util.verify`)
- Date/time formatting and arithmetic
- Markdown to HTML conversion
- External module imports via GitHub (`gh:bialet/extra/mcp`)
- MCP (Model Context Protocol) server support
- Built-in test framework with `bialet -T`
- Docker and Docker Compose support
- Cross-compilation for Windows (MinGW)

## Short Term

### v0.11

- [ ] **HTTPS / TLS support** — Native TLS in the server binary (no reverse
  proxy needed for basic deployments)
- [ ] **macOS live-reload** — File watching on macOS (currently Linux-only
  via inotify)
- [ ] **Request body size limits** — Configurable max upload size
- [ ] **Error page customization** — Override default 404/500 pages per app

### v0.12

- [ ] **Email sending** — SMTP client for transactional emails
- [ ] **Improved pagination** — Built-in offset/limit helpers for queries
- [ ] **Rate limiting** — Per-IP and per-route request rate limiting
- [ ] **Logging controls** — Configurable log levels and file output

## Medium Term

- [ ] **WebSocket support** — Basic WebSocket for real-time features
- [ ] **Background job queue** — Async task processing beyond Cron
- [ ] **Multi-app virtual hosting** — Serve multiple apps from one Bialet
  instance based on hostname
- [ ] **Template inheritance** — Nested layouts and partials in `_app.wren`
- [ ] **CLI scaffolding** — `bialet new <app>` to generate a starter project
- [ ] **Database migrations CLI** — `bialet migrate` commands for rollback
  and status

## Long Term / Ideas

- [ ] **PostgreSQL support** — Optional alternative database backend
- [ ] **Static file cache headers** — ETag / Last-Modified for `.css`, `.js`,
  images
- [ ] **i18n / localization** — Translation helpers and locale-aware
  formatting
- [ ] **Plugin system** — Community-contributed plugins installable via URL
- [ ] **Admin dashboard** — Built-in UI for browsing the database and viewing
  logs
- [ ] **WebAssembly target** — Compile Bialet apps to Wasm for edge
  deployment

## How to Contribute

See [CONTRIBUTING.md](CONTRIBUTING.md) for setup and guidelines.

Feature requests and feedback are welcome. Open an issue with the
"enhancement" label on
[GitHub Issues](https://github.com/bialet/bialet/issues).
