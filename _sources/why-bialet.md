# Why Bialet?

Bialet makes a specific trade-off: **maximum simplicity for a narrow but common
set of use cases**. This page explains what that means, when it's a good fit,
and how it compares to other tools.

## The Philosophy

Most web frameworks optimize for flexibility. They give you middleware stacks,
ORM layers, routing DSLs, template engines, and configuration files — all
designed to handle any scenario you might encounter.

Bialet optimizes for **getting things done on a single server**. It bundles the
HTTP server, the database, and the language runtime into one binary. Your app
is a directory of `.wren` files and a SQLite database. That's it.

If your needs fit within Bialet's scope, you get an experience that is hard to
assemble elsewhere: zero-config development, single-command deployment, and no
dependency management.

If your needs grow beyond that scope, you'll want a different tool — and that's
okay.

## When to Use Bialet

- **Prototypes and MVPs** — Go from idea to working app in minutes. No setup,
  no boilerplate.
- **Internal tools and dashboards** — Admin panels, reporting UIs, data
  browsers. The database is already there.
- **Simple CRUD applications** — Blogs, polls, task trackers, form-based apps.
- **Learning web development** — Skip the configuration and focus on HTML,
  SQL, and basic programming concepts.
- **Single-server deployments** — Small to medium traffic sites on a VPS or
  shared host.

## When NOT to Use Bialet

- **High-traffic public sites** — Bialet is a single-process server. It can't
  distribute load across multiple machines.
- **Real-time applications** — No WebSocket or Server-Sent Events support.
  Polling works for simple cases, but it's not ideal.
- **Large teams with existing infrastructure** — If you already have
  PostgreSQL, Redis, and a CI pipeline, Bialet's simplicity isn't buying you
  much.
- **Microservices** — Bialet is designed for monolithic apps on a single
  server.
- **Heavily interactive UIs** — Drag-and-drop editors, optimistic updates, and
  offline support need client-side state. Bialet re-renders the page instead.
- **Complex auth flows** — Bialet supports basic auth and sessions, but not
  OAuth, SAML, or multi-factor authentication out of the box.

## The Language Bet

Embedding Wren is the decision that makes Bialet Bialet. The VM is small,
dependency-free C, so it links into the same binary as the server and SQLite;
the language is class-based and object-oriented, so HTML, SQL, and application
logic live in one file without a build step.

It is also the biggest thing Bialet asks of you. There is no package registry,
no LSP or autocomplete yet, and most developers have never written Wren. The
[Wren guide](wren.md#what-wren-costs-you) states those costs plainly, and
[Wren for JavaScript and Python
developers](wren.md#wren-for-javascript-and-python-developers) is most of the
mapping you need to start.

## Comparisons

### Bialet vs. React / Next.js

React is a client-side UI library; Next.js is the framework around it, adding
routing, server rendering, and a build pipeline. Both are built to ship a
JavaScript application to the browser and keep it running there. Bialet renders
HTML on the server for every request and sends no component runtime at all.

|             | Bialet                           | React / Next.js                          |
| ----------- | -------------------------------- | ---------------------------------------- |
| Rendering   | Server-rendered HTML per request | Hydrated React tree; SSR/RSC via Next    |
| Client JS   | None required                    | React runtime plus app bundle            |
| Language    | Wren                             | JavaScript / TypeScript                  |
| Components  | Methods returning HTML strings   | Functions returning JSX                  |
| State       | URL and database; full reloads   | Hooks, context, stores; no reloads       |
| Data access | SQL in the request handler       | Server components, route handlers, fetch |
| Build step  | None                             | Bundler required                         |
| Deployment  | Single binary                    | Node server, container, or static host   |
| Ecosystem   | Minimal                          | npm — the largest in web development     |

React wins wherever the page has to react without a round trip: rich forms,
in-place editing, shared client state, offline behavior. It also wins on hiring
and libraries. Next.js pulls rendering back to the server, which narrows the
gap, but you still own the toolchain, the hydration model, and the bundle.
Bialet's inline HTML looks like JSX at a glance — [JSX to
Bialet](jsx-migration.md) lists the differences that matter when porting.

### Bialet vs. Vue

Vue is the gentler of the two mainstream client frameworks: single-file
components, a reactivity system, and Nuxt for server rendering. The difference
from Bialet isn't syntax — it's where state lives. In Vue the page is a
component tree kept up to date in the browser; in Bialet the page is
regenerated on the server and the state is whatever is in the database and the
URL.

|             | Bialet                           | Vue / Nuxt                              |
| ----------- | -------------------------------- | --------------------------------------- |
| Rendering   | Server-rendered HTML per request | Client-rendered with reactivity; SSR    |
| Client JS   | None required                    | Vue runtime plus app bundle             |
| Language    | Wren                             | JavaScript / TypeScript                 |
| Components  | Methods returning HTML strings   | Single-file components (`.vue`)         |
| State       | URL and database; full reloads   | Reactive refs, Pinia, Vuex              |
| Data access | SQL in the request handler       | `fetch`/axios, or server routes in Nuxt |
| Build step  | None                             | Vite required                           |
| Deployment  | Single binary                    | Static host, or Node server with Nuxt   |
| Ecosystem   | Minimal                          | npm, smaller than React's               |

For tables, forms, and links — most internal tools — Bialet does the same job
with no build step and no hydration. When you need live filtering, in-place
edits, or a genuinely app-like screen, Vue is the lighter of the two SPA
frameworks, but it is still an SPA. A server-rendered Nuxt app sits closest to
Bialet; the price is the toolchain you were trying to avoid.

### Bialet vs. Static Site Generators

Static site generators (Hugo, Jekyll, Eleventy) produce HTML files at build
time. Bialet generates HTML at request time and includes a database.

If your site has no dynamic data and no forms, use a static site generator.
If you need a database, forms, or user-specific content, Bialet is the
lighter alternative to a full backend framework.

### Bialet vs. PHP

PHP is the closest philosophical relative. Both let you drop files in a
directory and serve them immediately. Both embed logic in HTML.

|                 | Bialet                          | PHP                                |
| --------------- | ------------------------------- | ---------------------------------- |
| Language        | Wren (object-oriented)          | PHP                                |
| Database        | SQLite built in, always present | SQLite via `pdo_sqlite`, no server |
| Deployment      | Single binary + app directory   | PHP-FPM or Apache + app directory  |
| Package manager | None (import from GitHub URLs)  | Composer (optional)                |
| Template engine | Inline HTML strings             | Plain PHP, or Twig/Blade           |
| Configuration   | None                            | php.ini, .env, server config       |
| Shared hosting  | No                              | Yes                                |

`pdo_sqlite` is compiled into most PHP builds, so "PHP with SQLite" is a real
one-file setup that needs no database server. What Bialet adds is the
executable: no web server to configure, no php.ini, no FPM worker pool, and no
Composer step. What PHP adds is a hosting industry, a large ecosystem, and
plenty of people who already know it.

PHP wins on ecosystem, hiring, and hosting. Bialet wins on one artifact that
runs the same way on every machine.

### Bialet vs. Go

Go is the closest match on deployment: one static binary, an HTTP server in the
standard library, `database/sql`, and `html/template`. If a single binary is
the whole argument, Go already wins.

|            | Bialet                  | Go                              |
| ---------- | ----------------------- | ------------------------------- |
| Deployment | Single binary + app dir | Single binary                   |
| Concurrency| One request per worker  | Goroutines across all cores     |
| Database   | SQLite bundled          | A driver (CGO or pure Go)       |
| Templates  | Inline HTML + `{{ }}`   | `html/template` files           |
| Build step | None                    | `go build`                      |
| Types      | Dynamic                 | Static                          |
| Ecosystem  | Minimal                 | Large                           |

Go is the better tool when you need real parallelism, a typed codebase, or a
library for everything. Bialet is the better tool when you want to edit a
`.wren` file and refresh, keep one small binary, and not think about
compilation or dependencies at all.

### Bialet vs. PocketBase

PocketBase is also a single executable with SQLite behind it, but it is a
backend-as-a-service, not a template engine. It ships an admin UI, a collection
editor, auth, file storage, REST and realtime APIs, and a JavaScript SDK.
Bialet ships a language and expects you to write the HTML.

|            | Bialet                     | PocketBase                           |
| ---------- | -------------------------- | ------------------------------------ |
| What it is | Code-first, server-rendered| Backend with admin UI and APIs       |
| Data model | SQL schema you write       | Collections in the admin             |
| UI         | Your HTML                  | Built-in admin; you bring a frontend |
| Auth       | Sessions, CSRF, basic auth | Built-in auth, OAuth2 providers      |
| Realtime   | No (polling)               | SSE subscriptions                    |
| Extend     | `.wren` files              | Go or JavaScript hooks               |
| Database   | SQLite                     | SQLite                               |

Choose PocketBase when you want a ready-made backend and a JavaScript
frontend. Choose Bialet when the app is server-rendered pages and you want to
write the HTML and SQL yourself.

### Bialet vs. Express / Flask

Express and Flask are deliberately minimal: you add only what the app needs. A
minimal Express app can return HTML strings and talk to SQLite through a
driver; Flask ships Jinja2, and Python's standard library already has
`sqlite3`. Neither requires an ORM or a database server.

What neither includes is the wiring: sessions with CSRF protection, database
migrations, a request/response API, and a deployment story. Bialet's difference
is that all of it is in the binary and already connected.

|                 | Bialet                      | Minimal Express / Flask        |
| --------------- | --------------------------- | ------------------------------ |
| Setup           | One binary, zero config     | Runtime plus npm/pip install   |
| Database        | SQLite bundled              | SQLite via driver/stdlib       |
| Routing         | File-based                  | Decorators/router              |
| Templates       | Inline HTML + interpolation | Strings, or add EJS/Jinja2     |
| Sessions + CSRF | Built in                    | Sessions via middleware, CSRF via extension |
| Migrations      | `_migration.wren`           | Add a migration tool           |
| Production      | Single binary               | pm2 + nginx / gunicorn + nginx |
| Language        | Wren                        | JavaScript / Python            |

Express and Flask give you more control over the stack and a language people
already know. Bialet gives you fewer decisions and fewer moving parts.

### Bialet vs. HTMX plus a backend

If SPA complexity is the problem but a new language is not worth it, HTMX in
front of Flask, Express, or Go gives you partial updates without a JavaScript
build. That is a legitimate answer, and Bialet does not beat it on flexibility:
you keep a language and ecosystem you already know.

Bialet differs in what is already assembled — server, database, sessions, CSRF,
migrations — and in rendering whole pages by default instead of fragments. If
you already have a backend you like, adding HTMX is less work than adopting
Bialet. If you are starting from nothing on one server, Bialet removes more
pieces.

### Bialet vs. Django / Rails

Django and Rails are full-stack frameworks with ORMs, admin interfaces,
migrations, and extensive ecosystems. Bialet is full-stack in a different way:
it bundles the essentials, nothing more.

|                | Bialet                                       | Django / Rails                            |
| -------------- | -------------------------------------------- | ----------------------------------------- |
| Learning curve | Hours                                        | Weeks to months                           |
| ORM            | Raw SQL with Query objects                   | Django ORM / ActiveRecord                 |
| Admin panel    | Build your own                               | Django admin built-in / gems              |
| Migrations     | `_migration.wren` file                       | Versioned migration files                 |
| Scaling        | Multiple workers on one host (shared SQLite) | Horizontally via workers / load balancer  |
| Ecosystem      | Minimal                                      | Massive (packages / gems)                 |

If you're building a complex, long-lived application with a team, Django or
Rails are better choices. If you're building something simple today, Bialet
gets you there faster.

## The Trade-off

Bialet is intentionally limited. It doesn't have:

- Multi-server horizontal scaling
- WebSockets
- Background job queues (beyond Cron)
- A plugin ecosystem
- PostgreSQL / MySQL support
- An ORM
- OAuth or social login

What it does have is **everything you need to build a data-driven website in
a single binary**. For many projects, that's exactly the right trade-off.
