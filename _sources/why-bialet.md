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

If your needs fit within Bialet's scope, you get an experience no other
framework can match: zero-config development, single-command deployment, and
no dependency management.

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
- **Complex auth flows** — Bialet supports basic auth and sessions, but not
  OAuth, SAML, or multi-factor authentication out of the box.

## Comparisons

### Bialet vs. PHP

PHP is the closest philosophical relative. Both let you drop files in a
directory and serve them immediately. Both embed logic in HTML.

|                 | Bialet                           | PHP                               |
| --------------- | -------------------------------- | --------------------------------- |
| Language        | Wren (object-oriented)           | PHP                               |
| Database        | SQLite built-in, always present  | Must install and configure        |
| Deployment      | Single binary + app directory    | PHP runtime + web server + DB     |
| Package manager | None (import from GitHub URLs)   | Composer                          |
| Template engine | Inline HTML strings (JSX-like)   | Plain PHP or Twig/Blade           |
| Configuration   | None                             | php.ini, .env, server config      |

Bialet wins on simplicity. PHP wins on ecosystem and scalability.

### Bialet vs. Flask / Express

Flask and Express are minimalist frameworks, but they still need a database
driver, a template engine, and often an ORM — all configured separately.

|                 | Bialet                           | Flask / Express                   |
| --------------- | -------------------------------- | --------------------------------- |
| Setup           | One binary, zero config          | Install packages, configure DB    |
| Database        | SQLite built-in                  | SQLAlchemy + driver (Flask)       |
| Routing         | File-based                       | Decorators / router definitions   |
| Templates       | Inline HTML + interpolation      | Jinja2 / EJS / Pug                |
| Production      | Single binary                    | gunicorn + nginx / pm2 + nginx    |
| Language        | Wren                             | Python / JavaScript               |

Bialet eliminates the "glue code" phase entirely. Flask/Express give you more
control over the stack but require more decisions up front.

### Bialet vs. Rails / Django

Rails and Django are full-stack frameworks with ORMs, admin interfaces,
migrations, and extensive ecosystems. Bialet is full-stack in a different way:
it bundles the essentials, nothing more.

|                 | Bialet                           | Rails / Django                    |
| --------------- | -------------------------------- | --------------------------------- |
| Learning curve  | Hours                            | Weeks to months                   |
| ORM             | Raw SQL with Query objects       | ActiveRecord / Django ORM         |
| Admin panel     | Build your own                   | Built-in (Django admin) / gems    |
| Migrations      | `_migration.wren` file           | Versioned migration files         |
| Scaling         | Single process                   | Horizontally via workers/load bal |
| Ecosystem       | Minimal                          | Massive (gems / packages)         |

If you're building a complex, long-lived application with a team, Rails or
Django are better choices. If you're building something simple today, Bialet
gets you there faster.

### Bialet vs. Static Site Generators

Static site generators (Hugo, Jekyll, Eleventy) produce HTML files at build
time. Bialet generates HTML at request time and includes a database.

If your site has no dynamic data and no forms, use a static site generator.
If you need a database, forms, or user-specific content, Bialet is the
lighter alternative to a full backend framework.

## The Trade-off

Bialet is intentionally limited. It doesn't have:

- Horizontal scaling
- WebSockets
- Background job queues (beyond Cron)
- A plugin ecosystem
- PostgreSQL / MySQL support
- An ORM
- OAuth or social login

What it does have is **everything you need to build a data-driven website in
a single binary**. For many projects, that's exactly the right trade-off.
