# Product

<!-- impeccable:product-schema 1 -->

## Platform

web

## Users

Developers building a complete server-rendered app by themselves, typically on
one small server or VPS. The documented priority audiences, in order, are:

- **Internal tools and dashboards** — admin panels, reporting UIs, data
  browsers. The database is already there.
- **Simple CRUD apps** — guestbooks, polls, blogs, task trackers, form-based
  tools.
- **Prototypes and MVPs** — idea to working app without setup or boilerplate.
- **Learning and teaching web development** — HTML, SQL, and core logic instead
  of bundles, configs, and framework DSLs.
- **Single-server deployments** — small to medium traffic sites where copying
  one binary is the whole deploy.

Secondary audience: people handing Bialet to an AI assistant and expecting
idiomatic `.wren` output on the first try (the "Copy AI Prompt" flow).

*Inference:* the ranking comes from `docs/index.rst` ("Who is Bialet for?") and
`docs/why-bialet.md` ("When to Use Bialet"); no audience data or usage numbers
exist in the repository.

## Product Purpose

Bialet is a web framework in one small binary: an HTTP server, a SQLite
database, and the Wren scripting language, with no configuration files, no
package manager, and no build step. An app is a directory of `.wren` files plus
a `_db.sqlite3`, and deploying it means copying the executable.

Success looks like a working, persistent, data-driven web app that a single
developer can run, understand, and ship without provisioning infrastructure or
learning a toolchain. The framework is explicitly bounded rather than general:
`docs/why-bialet.md` publishes what it is *not* for (high-traffic public sites,
real-time features, microservices, complex auth flows).

## Positioning

The mechanism a neighboring framework could not truthfully copy: the URL path
*is* the route, resolved from the filesystem by probing for a `.wren` handler
named after each path segment; each handler returns an HTML body built inline
in the same language, with SQL written as backticked statements and Wren
expressions interpolated into the markup. All of it runs inside one C17
executable that embeds the Wren VM and SQLite — no runtime to install, no ORM,
no template engine, no middleware stack, no dependency tree.

Reference tagline, binding and verbatim: **"Web development became a spaceship.
Bialet is a bicycle."**

## Operating Context

- **Development:** `bialet dev` serves the current directory on
  `127.0.0.1:7001` with live reload, in-browser error display, and browser
  auto-open. On Windows, double-clicking `bialet.exe` starts the same dev mode.
- **Data:** SQLite file `_db.sqlite3` next to the app; migrations and cron
  tasks live in `_migration.wren` / `_cron.wren` (or under `/_app/`).
- **Deployment:** copy the binary to the server; no containers, orchestration,
  or `docker-compose.yml`. Static builds and a Windows cross-compile exist in
  the Makefile.
- **Install:** `curl -fsSL https://get.bialet.dev | sh` (macOS ARM, Ubuntu
  x86_64, Ubuntu ARM) or a per-platform release archive.
- **Documentation site:** Sphinx (`sphinx_book_theme`, MyST Markdown) built
  from `docs/` into `build/html`, published at `bialet.dev`. Sources are
  `.md`/`.rst`; the theme palette is overridden through
  `docs/_static/custom.css`.
- **Repository rituals:** `make` / `make check` (integration suite in
  `tests/run.sh`), clang-format enforced by a pre-commit hook, release notes in
  `CHANGELOG.md`, roadmap in `ROADMAP.md`.
- **Licensing and credits:** MIT; Wren and other upstream projects are credited
  in the README.

## Capabilities and Constraints

Confirmed capabilities: filesystem-based routing with `_app.wren` site
templates; inline HTML with `{{ }}` interpolation; prepared SQL queries via
backticked statements; built-in SQLite with WAL option; forms and multipart
upload handling; sessions and basic auth; outbound HTTP calls; cron; markdown
rendering; modules imported from GitHub URLs; live reload; memory and CPU
limits; a test runner (`bialet -T`); syntax validation (`bialet -t`).

Confirmed constraints, all documented as trade-offs rather than bugs:

- Single process, single server — no horizontal scaling or load distribution.
- No WebSocket or Server-Sent Events support; polling only.
- SQLite is the only database; no PostgreSQL, MySQL, or external DB servers.
- No OAuth, SAML, or multi-factor auth out of the box.
- No package manager, bundler, or config file layer by design; there is no
  plugin ecosystem.
- Written in C17; runs on Linux, macOS, and Windows.
- Values read from the database arrive as strings and must be converted before
  numeric math; missing `Request.post()` keys return `null`.

Terminology that must stay exact: **Bialet** (capital B, never an acronym),
`bialet` (the CLI binary, lowercase), **Wren** (the language, capital W),
`.wren` (the extension, lowercase, leading dot), **Ride Light** (the manifesto,
both words capitalized).

## Brand Commitments

`BRAND.md` at the repository root is binding for the website, README, social
previews, talks, and third-party material.

- **Philosophy:** do less, on purpose. Clarity over decoration, plain over
  slick, honest over hyped. Every color, icon, and word has a job.
- **Symbol:** one line-art bicycle, dark navy/slate on a light or transparent
  circle. It is never redrawn, recolored, shadowed, or paired with a second
  mascot. The 🚲 emoji is a text-only stand-in used once as a title marker.
- **Palette:** two modes, 3-character hex only. Light `--bg #FFF`, `--text
  #024`, `--link #06F`, `--link-hover #04F`, `--code #B00`; dark `--bg #024`,
  `--text #FFF`, `--link #0BF`, `--link-hover #0FF`, `--code #FA0`. No third
  accent color, no 6-digit hex where a 3-digit one is identical.
- **Typography:** `system-ui` stack for text and UI, monospace stack for code.
  No web fonts, no build step. The large centered hero style belongs to the
  landing page only; documentation prose stays left-aligned at body size.
- **Iconography:** plain outline Octicons, one per concept, rendered in
  `--text`. No filled/outline mixing, no utility colors, and never a rocket or
  spaceship icon — that is the thing Bialet is defined against.
- **Voice:** plain, technical, no-nonsense; short declarative sentences, second
  person, code-first, dry. The spaceship/bicycle contrast is the ceiling for
  playfulness. Pitfall callouts instead of vague warnings.

## Evidence on Hand

- Real, runnable code samples throughout `README.md` and `docs/` (guestbook,
  poll app with screenshots in `docs/getting-started/`).
- Screenshots of a real tutorial app: `docs/_static/poll-vote.png`,
  `docs/_static/poll-results.png`.
- Logo and social assets: `docs/_static/logo.png`, `docs/_static/og-image.png`,
  `src/favicon.ico`.
- A complete 40+ page documentation set including honest comparison tables
  (`docs/why-bialet.md`), an FAQ, a security guide, and a reference.
- An AI prompt generated at build time from `docs/prompt-guide.md`.

Absent, and must not be invented: named customers, testimonials, logos of
companies using Bialet, benchmarks or performance numbers, download or usage
statistics, and pricing (the project is MIT and free).

## Product Principles

1. **Ship less tooling, not more.** Any feature that adds a setup step has to
   justify itself against the cost of the line the user no longer writes.
2. **Standards are the interface.** HTML, SQL, and HTTP are the surface; the
   framework stays a thin, predictable layer over them.
3. **Honest scope beats broad appeal.** Publish who the product is not for and
   keep the trade-offs visible in the docs.
4. **One artifact to deploy.** The unit of distribution and the unit of
   deployment stay a single binary plus the app directory.
5. **Plain is the aesthetic.** Restraint in color, type, iconography, and copy
   is the brand, not a phase before a redesign.

## Accessibility & Inclusion

Both color modes must meet WCAG AA (4.5:1 for text, 3:1 for large text and UI)
before a new color combination ships — stated in `BRAND.md`, and mapped onto
the theme tokens in `docs/_static/custom.css`. No other product-specific
accessibility requirement or audited standard has been established.
