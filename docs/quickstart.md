# Quick start: a guestbook

Build and run a guestbook in about five minutes. Messages are stored in SQLite,
rendered on the server, and returned as HTML — with no build step, no package
manager, and no configuration file.

The app lives in [docs/examples/guestbook](https://github.com/bialet/bialet/tree/main/docs/examples/guestbook)
if you would rather read it than type it.

## Before you start

You need `bialet` on your `PATH`. See [Installation](installation.md) for
other platforms.

```bash
curl -fsSL https://get.bialet.dev | sh
```

```bash
mkdir guestbook && cd guestbook
```

## 1. The table

`_migration.wren` runs every time the app starts and applies only what it has
not applied before. The name you give a migration is its key, so `init` runs
once, on the first start.

```{literalinclude} examples/guestbook/_migration.wren
:language: wren
```

The database is a file next to your app (`_db.sqlite3`), created on first run.
There is no service to start and no connection string.

## 2. The page

`index.wren` is the page at `/`. The file name is the route.

```{literalinclude} examples/guestbook/index.wren
:language: wren
```

Three things worth noticing:

- Backticks build a **prepared statement**, not a string. `.fetch` runs it and
  returns the rows as a list of maps.
- `<main>…</main>` is an **inline HTML string**. Its value is the response body.
- `{{ messages.map {|m| … } }}` is a Wren expression. Plain values are escaped
  automatically, so text from the database cannot inject markup.

> **Pitfall:** column values always come back as **strings**, including numbers.
> Convert before doing math — `Num.fromString(row["count"])`, or `.toNum` when a
> query returns a single number.

## 3. The form handler

`message.wren` is the URL `/message`. It is a separate file because it does a
different job, not because it needs wiring: no router, no table of routes.

```{literalinclude} examples/guestbook/message.wren
:language: wren
```

- `Request.post("msg")` returns **`null`** when the field is missing. Check it
  before using it, or the next string operation crashes at runtime.
- `?` is a placeholder. Values are passed to `.query()`, never concatenated into
  the SQL, so injection is not a thing you can do by accident.
- `Response.redirect` sends a 302. `return` stops the script so no body is
  attached to the redirect.

## Run it

```bash
bialet dev
```

`bialet dev` serves the directory on 127.0.0.1:7001, opens it in your browser,
applies the migration, and reloads the page when a file changes. Errors show up
in the browser instead of only in the terminal. Drop `dev` for the same server
without live reload, error display, or auto-open.

Post a message and watch it appear. The message is in SQLite: delete
`_db.sqlite3` to start over, and the next start recreates it and reruns the
migrations.

That is a working app. It also looks like 1995, which is the next problem.

## 4. A stylesheet

Add a plain stylesheet. Bialet does not care where static files live, as long as
the path does not start with `_` or `.`; this app uses `css/style.css`. The file
is ordinary CSS — custom properties, a gradient, one card:

```{literalinclude} examples/guestbook-styled/css/style.css
:language: css
:lines: 1-30
```

The rest of the file styles the composer, the button, and the message list. The
whole thing is [112 lines](https://github.com/bialet/bialet/blob/main/docs/examples/guestbook-styled/css/style.css).

## 5. One layout class

Two pages now share a `<head>` and a card wrapper. That belongs in one place.
Protected files start with `_`, so `_app.wren` is importable from any page and
never served as a URL:

```{literalinclude} examples/guestbook-styled/_app.wren
:language: wren
```

`{{ content }}` is where each page's markup lands. Naming the file `_app.wren`
is a convention, not a requirement — `import "_app" for Template` is the whole
connection.

## 6. Use it

Add the import and wrap the page in `Template.layout(...)`:

```{literalinclude} examples/guestbook-styled/index.wren
:language: wren
```

```{literalinclude} examples/guestbook-styled/message.wren
:language: wren
```

Reload. Same routes, same database, same SQL — now inside a layout that loads
the stylesheet. For the record, the URL bar showing `?new=1` is what makes the
newest message keep its highlight for one render; it is a normal query string,
read with `Request.get("new")`.

## What just happened

- **The file is the route.** `index.wren` → `/`, `message.wren` → `/message`.
  Nothing declares it.
- **SQL is SQL.** Backticked statements are prepared; `?` binds parameters.
- **HTML is a value.** Pages return inline HTML strings; `{{ }}` inserts
  expressions and escapes them.
- **One binary.** `bialet` served the files, opened the database, and applied the
  migration. Nothing was compiled, installed, or bundled.

The landing page [video](index.rst) walks through the same two versions of this
app.

## Where to go next

- [Tutorial: build a poll](getting-started/index.md) — the longer tour, with
  templates, forms, and results pages.
- [Database](database.md) — queries, `.order()`, pagination, migrations.
- [Forms](forms.md) — POST handling, files, CSRF.
- [HTML Template](template.md) — inline HTML in depth.
- [Advanced Routing](advanced-routing.md) — when a URL needs path segments.
- [AI Coding Prompt](ai-prompt.md) — paste it into Claude, ChatGPT, or Cursor
  before your next app, or install the `bialet-app-builder` skill.
