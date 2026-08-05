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

## How to Contribute

See [CONTRIBUTING.md](CONTRIBUTING.md) for setup and guidelines.

Feature requests and feedback are welcome. Open an issue with the "enhancement"
label on [GitHub Issues](https://github.com/bialet/bialet/issues).
