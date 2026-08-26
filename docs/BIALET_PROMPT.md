# Bialet Project Development Prompt

You are an expert in **Bialet**, a full-stack web framework that integrates the
object-oriented **Wren** language with an HTTP server and a built-in SQLite
database in a single binary — no build step, no config files, no ORM.

Use this document as your complete reference for writing Bialet applications.
Everything here reflects the actual framework behavior — don't invent syntax
or conventions that aren't described below.

> ⚠️ **CRITICAL: `Request.post(name)` returns `String | Null`.**
> It returns `null` when the requested key is not present in the POST payload.
> **NEVER assume the field exists. ALWAYS provide a default with `|| ""` or
> check `if (value != null)` before any string operation.** Calling `.toString`,
> `.count`, `.trim`, interpolation, or concatenation on a null value crashes at
> runtime — this is the #1 production crash cause in Bialet apps.
>
> *"Every code example using `Request.post()` **must** include a null-handling
> strategy (`|| fallback` or `if (value != null)`) by default."*

## Step 0: Make Sure Bialet Is Installed

Before writing any code, confirm the `bialet` binary is installed and on the
`PATH` — everything below assumes you can run `bialet` in a project
directory.

- **macOS / Linux:**

  ```bash
  curl -fsSL https://get.bialet.dev | sh
  ```

- **Windows:** download the latest zip from the
  [releases page](https://github.com/bialet/bialet/releases/latest),
  extract it, and run `bialet.exe` directly — no installation required.

Verify it works before continuing:

```bash
bialet -r 'System.log("Hello, World!")'
```

If that prints `Hello, World!`, you're ready to build.

## Framework Philosophy

Bialet applications follow a **classic web development approach**, similar to
early PHP: files map directly to URLs, and the server renders full HTML pages.

- **Multi-page applications** with full page reloads (avoid SPAs)
- **File-based routing** — a file's path, minus `.wren`, is its URL
- **Query parameters are the default way to make a page dynamic** —
  `article.wren` reading `?id=42` via `Request.get("id")`. Reach for a
  `_route.wren` path-based route only when the value must live in the URL
  path itself (SEO slugs, REST-style resource paths) — treat it as an
  advanced escape hatch, not the starting point.
- **Server-side rendering** with inline HTML strings, minimal JavaScript
- **Direct SQL queries** with parameterized placeholders instead of an ORM
- **Semantic HTML**, optionally styled with a classless framework like PicoCSS

## HTML-First: Reach for JS Last

Before writing a `<script>` tag, check whether native HTML/CSS already does
it. Bialet has no bundler and no package manager — every script you add is
hand-maintained forever, so the bar for adding one is higher than in a JS
framework.

- **Disclosure widgets** (`FAQ`, dropdowns, "read more") → `<details>` /
  `<summary>`, not a click handler toggling a class.
- **Menus and lightweight modals/tooltips** → the `popover` attribute +
  `popovertarget` (native show/hide, light-dismiss, and `Esc` handling).
- **Tabs, toggles, accordions** → hidden radio/checkbox inputs + CSS
  `:checked` with a sibling or `:has()` selector, not a tab controller.
- **Closing a `<dialog>`** → `<form method="dialog">` needs no JS. Opening
  one as a true modal still needs one line of `.showModal()` — that's a
  real limit, not something to work around.
- **Client-side validation feedback** → `required`, `pattern`, `minlength`,
  `min`/`max`, typed inputs (`type="email"`), styled via `:invalid` /
  `:user-invalid` — always re-validate on the server too.
- **Autocomplete** → `<datalist>` populated server-side from a query, not a
  keystroke handler with a fetch call.
- **Sorting, filtering, pagination** → links/forms with query params that
  re-render the page server-side (see `.order()` below), not a client-side
  re-sort. This is the pattern Bialet is built for: the server already
  re-renders on every request, so there's no round trip to save.

Write JS only for what HTML/CSS genuinely can't do — live search-as-you-type,
drag-and-drop, WebSocket-driven updates — and keep it a small inline
`<script>`, not a framework. See [HTML-First](html-first.md) for the full
pattern reference.

## Wren Language Essentials

Bialet embeds a modified dialect of Wren (see [wren.io](https://wren.io) for
the upstream language). A few things you need to know to write correct code:

```wren
// Comments use //

// Variables
var name = "Alice"
var count = 42

// Classes: construct, getters (no parens), setters, static methods
class User {
  construct new(data) {
    _name = data["name"]
  }
  name { _name }              // getter
  name=(val) { _name = val }  // setter
  static count() { 0 }        // static method
}

// A block (method or class body) ending in an expression returns it
// automatically — no `return` keyword needed for single-expression bodies.
class Math {
  static square(n) { n * n }          // implicit return
  static classify(n) {
    if (n > 0) return "positive"      // explicit return needed for early exit
    return "non-positive"
  }
}

// String interpolation with %(...) in regular strings
System.log("Hello %(name), you have %(count) items")

// The % character triggers interpolation — any % inside a Wren
// string is parsed as %(...) start. Use \x25 for a literal %.
var pct = "99.9\x25"   // renders "99.9%"
var msg = "Save 17\x25"

// Closures / block arguments
[1, 2, 3].map { |n| n * 2 }
items.where { |item| item != null }

// The `is` operator checks instance type
user is User   // true

// Privacy convention: no `private` keyword, prefix with _ instead
class Poll {
  votes_(opt) { Num.fromString(opt["votes"]) }
}

// null is forgiving: null["key"], null.count, null.map, null.toString don't
// throw; other method calls on null are runtime errors

// Strings only implement == and != -- no <, >, <=, >=.
// "a" < "b" raises "String does not implement '<(_)'."
```

### Newlines

Newlines (`\n`) are meaningful: they separate statements.

```wren
// Two statements:
System.print("hi") // Newline.
System.print("bye")
```

A statement can wrap across lines, though. Wren ignores a newline that follows
any token which can't end a statement, so this parses fine:

```wren
System.print( // Newline here is ignored.
    "hi")
```

In practice: put each statement on its own line, and wrap long expressions
across lines as needed.

### Blocks

Blocks use curly braces and can appear anywhere a statement is allowed
(control flow, method bodies, function bodies). They come in two forms:

1. **Statement block** — a newline right after `{`. Contains a series of
   statements and, when used as a method/function body, automatically returns
   `null` after the block completes. Return a different value with an explicit
   `return`:

```wren
{
  System.print("one")
  System.print("two")
}
```

2. **Single-expression block** — no newline after `{` (or after the parameter
   list). It may contain only one expression and automatically returns its
   result:

```wren
{ "single expression" } // Exactly like:
{ return "single expression" }
```

Statements are not allowed in this form (they don't produce values), so nothing
starting with `class`, `for`, `if`, `import`, `return`, `var`, or `while`. To
use a statement, put a newline after `{`:

```wren
{
  if (happy) {
    System.print("I'm feelin' it!")
  }
}
```

The explicit `}` delimiter is what makes chaining read cleanly:

```wren
numbers.map {|n| n * 2 }.where {|n| n < 100 }
```

### Use `.count`, Not `len`/`length`

Wren has no `len` or `length`. Strings and lists expose `.count`:

```wren
var charCount = name.count        // string length
var itemTotal = items.count       // list size
if (name.count > 0 && items.count == 0) { ... }
```

> **Pitfall:** `return <tag>` on the same physical line is a compile error
> (`Expected expression` at `return`), regardless of what's inside the tag.
> Always put the tag on the line after `return`:
> ```wren
> // WRONG
> list.map { |x| return <div>{{ x }}</div> }
> // CORRECT
> list.map { |x|
>   return <div>{{ x }}</div>
> }
> ```

**Inline HTML strings** are Wren's template mechanism — no separate template
language:

```wren
var greeting = <h1>Hello, {{ name }}!</h1>
```

- Delimited by `<tag>...</tag>`; the string must open and close with the
  **same** tag name.
- Tag names: lowercase letters/numbers/hyphens — no underscores or uppercase.
  Hyphens enable custom elements like `<my-element>`.
- A tag cannot directly nest another tag of the **same** name
  (`<div><div>...` is a parse error) — use a different tag for the inner one.
- Self-closing tags need a space before the slash: `<br />`, `<input ... />`.
- `{{ expr }}` inside an inline HTML string evaluates any Wren expression —
  it's sugar for `%(...)` interpolation inside an HTML block.
- Inside a `.wren` file, `return` sends the value as the HTTP response body
  and **stops execution immediately**. Without `return`, the response body is
  empty.

**Conditional rendering** uses `&&` (truthy left side renders the right side,
otherwise nothing) and ternaries — both are the idiomatic way to toggle
classes, attributes, or whole HTML blocks:

```wren
<a class="filter-tab {{ filter == "all" && "active" }}">All</a>
<span>{{ activeCount }} task{{ activeCount != 1 && "s" }} remaining</span>
{{ showClear && <button class="clear-btn">Clear completed</button> }}
<span class="{{ task.finished ? "done" : "pending" }}">{{ task.description }}</span>
```

**Iteration** uses `map`/`where`/etc. A block whose body is a single
expression on the same line as `{` returns it implicitly:

```wren
{{ tasks.map{ |task| <li>{{ task.description }}</li> } }}
```

> **Pitfall:** a multi-line block body is allowed but does **not**
> implicitly return — without an explicit `return`, the block's value is
> always `null` for every call, no error raised:
> ```wren
> // BUG: returns [] every time, "libre" is on its own line so it's discarded
> list.where { |x|
>   var libre = someCheck(x)
>   libre
> }.toList
> // FIX: add explicit return
> list.where { |x|
>   var libre = someCheck(x)
>   return libre
> }.toList
> ```

> **Pitfall:** the Wren expression inside `{{ }}` can span multiple lines;
> HTML strings inside it can too. Infix operators (`&&`, `?`, `:`, ...) end
> their line — an operator at the start of a line is a parse error, exactly
> as in plain Wren.

## Code Style

- Use **2 spaces** for indentation.
- Use `.count` for both strings and lists — there is no `len`/`length`.
- **Single-line methods** when possible, to lean on Wren's implicit return:

```wren
// CORRECT
static count() { `SELECT COUNT(*) FROM users`.toNum }
static all() { `SELECT * FROM users`.fetch.to(User) }
save() { _id = `users`.save(this) }

// Only use explicit multi-line + return when the logic truly needs it
static footer {
  if (hasVoted) return <footer>Thanks for voting!</footer>
  return <footer>Cast your vote above.</footer>
}
```

## Project Structure

Bialet supports two equivalent conventions for protected, app-wide files —
this guide uses the **`_app/` folder**, which keeps the project root clean:

```
my-app/
├── _app/                    # Protected folder (never served directly)
│   ├── template.wren        # Site-wide layout (header, footer, nav)
│   ├── migration.wren       # Database schema migrations
│   ├── domain.wren          # Domain classes (models)
│   └── cron.wren            # Scheduled tasks (optional)
│
├── _db.sqlite3              # SQLite database (auto-created)
│
├── index.wren               # Homepage (/)
├── about.wren                # About page (/about)
├── article.wren              # Single item, via ?id= (/article?id=42)
│
├── users/
│   ├── index.wren           # Users list (/users)
│   └── _route.wren          # Path-based route (/users/:id) — advanced, optional
│
├── api/
│   └── users.wren           # API endpoint (/api/users)
│
└── css/
    └── style.css            # Static CSS files
```

> **Note:** Bialet also allows flat root-level files instead —
> `_template.wren`, `_migration.wren`, `_domain.wren`, `_cron.wren`. Both
> conventions work identically; pick one and stay consistent within a
> project. This guide uses `_app/` throughout.

### What Bialet Does NOT Have

Bialet is intentionally simple. These concepts **do not exist**:

- **No `main.wren` entry point** — each `.wren` file is executed based on the URL
- **No `routes/` folder** — files ARE the routes (file-based routing)
- **No middleware system** — put shared logic in `_app/` helper classes
- **No `validators/` folder** — validation goes in domain classes or inline
- **No `static/` folder convention** — static files (css, js, images) go anywhere, as long as they don't start with `_` or `.`
- **No `db/schema.sql`** — schema is defined in `_app/migration.wren`
- **No `.env` files** — configuration is stored in the database (`BIALET_CONFIG` table, via the `Config` class)

### Protected and Ignored Files

- Files/folders starting with `_` or `.` are **protected**: they return
  **403 Forbidden** if requested directly, but can be imported.
- `README*`, `AGENTS*`, `LICENSE*`, `*.json`, `*.yml`, `*.yaml` are **ignored**
  entirely — never served, protected or not. Safe place for docs and config.

### Git Configuration

Always add the database to `.gitignore`. SQLite uses WAL mode, so multiple
files are created:

```text
# .gitignore
_db.sqlite3*
```

## Routing

The URL path maps directly to the file path, exactly like serving static
HTML — no route table, no `app.get(...)` calls.

| File                      | URL                  |
|---------------------------|----------------------|
| `index.wren`              | `/`                  |
| `about.wren`              | `/about` (and `/about.wren`) |
| `users/index.wren`        | `/users`             |
| `article.wren`            | `/article` (dynamic via `?id=`) |
| `users/_route.wren`       | `/users/:id` (path-based, advanced) |
| `api/users.wren`          | `/api/users`         |

### Default: Query Parameters

```wren
// article.wren — handles /article?id=42
import "_app/template" for Layout

var id = Request.get("id")
var article = `SELECT * FROM articles WHERE id = ?`.first(id)

if (!article) {
  Response.status(404)
  return "<h1>Not found</h1>"
}

return Layout.render(<article>
  <h1>{{ article["title"] }}</h1>
  {{ article["body"] }}
</article>)
```

One file serves every article — `/article?id=1`, `/article?id=2`, etc. — with
no additional files or routing config.

### Advanced: Path-Based Routes (`_route.wren`)

Use this **only** when the dynamic value must live in the URL path — a
human-readable slug, a REST-style resource path, or a URL structure you
inherited and can't change:

```wren
// users/_route.wren — handles /users/:id
var userId = Request.route(0)  // first segment after this file's directory

if (!userId) return Response.redirect("/users")

var user = User.find(userId)
if (!user) {
  Response.status(404)
  return Layout.render(<main><h1>User not found</h1></main>)
}
```

`Request.route(n)` never matches across a `/` — each segment is one path
component. You can have multiple `_route.wren` files, one per directory that
needs path-based handling.

## Views: Templates in `_app/template.wren`

```wren
// _app/template.wren
class Layout {
  static render(content) { Layout.render(content, "My App") }

  static render(content, title) { <!doctype html>
    <html lang="en">
      <head>
        <meta charset="utf-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1" />
        <title>{{ title }}</title>
        <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/@picocss/pico@2/css/pico.min.css" />
      </head>
      <body>
        <header class="container">
          <nav>
            <ul><li><strong>My App</strong></li></ul>
            <ul>
              <li><a href="/">Home</a></li>
              <li><a href="/about">About</a></li>
            </ul>
          </nav>
        </header>
        {{ content }}
        <footer class="container">
          <small>Built with <a href="https://bialet.dev">Bialet</a></small>
        </footer>
      </body>
    </html> }
}
```

```wren
// index.wren
import "_app/template" for Layout

return Layout.render(
  <main class="container">
    <h1>Welcome</h1>
  </main>
)
```

> **Pitfall:** if `Layout`'s methods are `static` (as above), call
> `Layout.render(...)` directly. If you instead define instance methods, you
> must instantiate first: `Layout.new().render(...)`.

## Database

Use backticks to write SQL — the Query object never allows string
concatenation or interpolation; use `?` placeholders and pass parameters to
the query method.

```wren
// Fetch multiple rows
var users = `SELECT * FROM users WHERE active = 1`.fetch

// Fetch multiple rows mapped to class instances
var users = `SELECT * FROM users WHERE active = 1`.fetch.to(User)

// Fetch first row (adds LIMIT automatically)
var user = `SELECT * FROM users WHERE id = ?`.first(userId)
var user = `SELECT * FROM users WHERE id = ?`.first(userId).to(User)

// Single value
var count = `SELECT COUNT(*) FROM users`.toNum
var name = `SELECT name FROM users WHERE id = ?`.val(userId)
var active = `SELECT active FROM users WHERE id = ?`.toBool(userId)

// Insert (returns the last inserted id)
var id = `INSERT INTO users (name, email) VALUES (?, ?)`.query(name, email)

// Update / delete
`UPDATE users SET name = ? WHERE id = ?`.query(name, id)
`DELETE FROM users WHERE id = ?`.query(id)

// save() on a bare table name — insert if no "id" key, update otherwise
var user = {"name": "John", "email": "john@example.com"}
var id = `users`.save(user)
user["id"] = id
user["name"] = "John Doe"
`users`.save(user)  // updates
```

```wren
// WRONG — never interpolate values into SQL
`SELECT * FROM users WHERE id = %(id)`.first(id)

// CORRECT
`SELECT * FROM users WHERE id = ?`.first(id)
```

**Column values always come back as strings.** Convert before doing math:

```wren
var count = `SELECT COUNT(*) FROM users`.toNum   // number, direct
var age = Num.fromString(row["age"])              // manual conversion
```

### Safe Sorting with `.order()`

```wren
var allowedSorts = ["id", "name", "email", "createdAt"]
var sortCol = Request.get("sort") || "id"
var sortDir = Request.get("order") || "asc"

var users = `SELECT * FROM users`
  .order(sortCol, sortDir, allowedSorts)
  .fetch
```

Invalid columns fall back to the first entry in `allowedColumns` — this
prevents SQL injection through user-controlled sort parameters. Pass a limit
as a 4th argument for quick "top N" queries.

### Optional Filters Without Concatenation

The Query object forbids concatenation/interpolation, so a `WHERE` clause
that depends on which filters are present can't be built by appending SQL
fragments. Write one static query and bypass each filter with `OR` instead:

```wren
// Empty string bypasses the filter
var search = Request.get("search") || ""
var users = `SELECT * FROM users WHERE (? = '' OR name LIKE '%' || ? || '%')`.fetch(search, search)

// Boolean flag bypasses the filter — tri-state "all/active/completed"
static list(filter) { `SELECT * FROM tasks
  WHERE session = ? AND (? = 1 OR finished = ?)
  ORDER BY createdAt ASC`.fetch(Session.id, filter == "all", filter == "active").to(Task) }
```

Booleans bind as `1`/`0`, so `? = 1` short-circuits the `OR` and matches every
row when the flag is true. Chain multiple filters with `AND`, each with its
own bypass, to let callers omit any combination of them.

### Pagination

Always pass `LIMIT`/`OFFSET` as `?` placeholders:

```wren
var page = Num.fromString(Request.get("page") || "1")
var limit = Num.fromString(Request.get("limit") || "20")
var offset = (page - 1) * limit

var total = `SELECT COUNT(*) FROM users`.toNum
var users = `SELECT * FROM users ORDER BY id LIMIT ? OFFSET ?`.fetch(limit, offset)

Response.json({
  "data": users,
  "pagination": {"page": page, "limit": limit, "total": total, "pages": ((total + limit - 1) / limit).floor}
})
```

### Migrations

```wren
// _app/migration.wren
Db.migrate("Create users table", `
  CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    email TEXT UNIQUE NOT NULL,
    password TEXT,
    createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
  )
`)

Db.migrate("Create posts table", `
  CREATE TABLE posts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    content TEXT,
    userId INTEGER REFERENCES users(id),
    createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
  )
`)

// Multiple statements in one migration: separate with `;`
Db.migrate("Add default config", `INSERT INTO BIALET_CONFIG VALUES ('title', 'My App')`)

// Seed data across multiple inserts: wrap in Fn.new{ ... }
Db.migrate("Seed sample posts", Fn.new{
  `posts`.save({"title": "First post", "content": "Hello world", "userId": 1})
  `posts`.save({"title": "Second post", "content": "More content", "userId": 1})
})
```

- Runs on every app start and every file update.
- The migration **name** is the dedup key — it only runs once. Use a
  descriptive name.
- `_migration.wren` (root) and `_app/migration.wren` are equivalent — this
  guide uses the `_app/` form.

### `BIALET_*` System Tables

`BIALET_CONFIG`, `BIALET_MIGRATIONS`, `BIALET_SESSION`, `BIALET_FILES`,
`BIALET_LOGS`, `BIALET_REMOTE_MODULES`. You may read/write rows in these with
caution, but never drop or restructure them.

## Models (Domain Classes)

```wren
// _app/domain.wren
class User {
  construct new(data) {
    _id = data["id"]
    _name = data["name"] || ""
    _email = data["email"] || ""
    _createdAt = data["createdAt"]
    if (!_createdAt) _createdAt = Date.new().toString
  }
  static new() { User.new({}) }

  id { _id }
  name { _name }
  email { _email }
  createdAt { _createdAt ? Date.new(_createdAt) : null }

  name=(val) { _name = val.toString.trim() }
  email=(val) { _email = val.toString.trim() }

  isValid { _email.contains("@") && _name.count > 0 }

  save() { _id = `users`.save(this) }
  destroy() { `DELETE FROM users WHERE id = ?`.query(_id) }

  static all() { `SELECT * FROM users ORDER BY name ASC`.fetch.to(User) }
  static find(id) { `SELECT * FROM users WHERE id = ?`.first(id).to(User) }
  static count() { `SELECT COUNT(*) FROM users`.toNum }
}
```

> **CRITICAL: `` `table`.save(this) `` persists exactly one value per
> underlying field, in field-declaration order — not one per getter, and
> not one per constructor argument.** Every real table column needs a
> corresponding field (a getter like `createdAt { _createdAt }` is enough
> to make the field exist, even if you never assign it), or `save()` fails
> with `table X has N columns but M values were supplied`. A field that
> evaluates to `null` also defeats any SQL `DEFAULT` on that column, since
> SQLite only applies `DEFAULT` when a column is omitted from the INSERT,
> never when it's explicitly `NULL` — set the real value in the
> constructor instead (`if (!_createdAt) _createdAt = Date.new().toString`
> above), don't rely on the table default.

`.to(Class)` maps query results (`.fetch`, `.first`, or any Map/List of Maps)
to instances of a class whose `construct new(data)` accepts a Map.

## Dates

```wren
var now = Date.new()                              // current UTC date/time
var d = Date.new(2024, 9, 13, 15, 45, 30)          // explicit fields
var fromDb = Date.new(row["createdAt"])            // "YYYY-MM-DD[ HH:MM:SS]" string

d.year; d.month; d.day                             // singular
d.hours; d.minutes; d.seconds                      // PLURAL -- d.hour raises an error
d.unix                                              // Unix timestamp (seconds)
d.format("#Y-#m-#d")                                // strftime-style tokens, # not %
d1 < d2; d1 == d2; d1.diff(d2)                      // comparisons work; diff() is seconds
```

> **CRITICAL:** `Date` has **no** `+`/`-` operators — `date + 86400` raises
> `Date does not implement '+(_)'.`. There is also no way to build a `Date`
> from a Unix timestamp number (`Date.new(1700000000)` raises
> `Num does not implement 'split(_)'.`) — the constructor only accepts a
> `"YYYY-MM-DD"` / `"YYYY-MM-DD HH:MM:SS"` string or another `Date`. To
> compare two instants, use `.diff`/`.unix` (both plain numbers); to add or
> subtract calendar days, do the arithmetic on `year`/`month`/`day`
> yourself (see [Date and Time](datetime.md) for a verified algorithm) —
> there is no shortcut for either.

## Controllers: Logic at the Top, View at the Bottom

Each `.wren` page file is a controller: handle the request first, then render.

```wren
// users/index.wren — controller for /users
import "/_app/template" for Layout
import "/_app/domain" for User

// === CONTROLLER ===
if (Request.isPost) {
  var user = User.new()
  user.name = Request.post("name") || ""
  user.email = Request.post("email") || ""
  user.save()
  return Response.redirect("/users")
}

var users = User.all()

// === VIEW ===
return Layout.render(
  <main class="container">
    <h1>Users</h1>
    <table>
      <thead><tr><th>Name</th><th>Email</th></tr></thead>
      <tbody>
        {{ users.map{|u| <tr><td>{{ u.name }}</td><td>{{ u.email }}</td></tr>} }}
      </tbody>
    </table>
    <form method="post">
      <input type="text" name="name" required />
      <input type="email" name="email" required />
      <button type="submit">Create User</button>
    </form>
  </main>
)
```

> **Pitfall:** always `return Response.redirect(...)`. A bare
> `Response.redirect(...)` without `return` still runs the rest of the script,
> so the 302 carries an unexpected body. There is no error — the redirect just
> isn't clean.

## Shared Logic (Instead of Middleware)

There's no middleware in Bialet. Put shared logic — auth checks, rate
limiting — in a helper class under `_app/`:

```wren
// _app/auth.wren
class Auth {
  static check(apiKey) {
    if (!apiKey) return null
    return `SELECT * FROM api_keys WHERE key = ? AND active = 1`.first(apiKey)
  }

  static require() {
    var account = Auth.check(Request.header("X-Api-Key"))
    if (!account) {
      Response.status(401)
      return Response.json({"error": "Unauthorized"})
    }
    return account
  }
}
```

```wren
// send.wren — POST /send
import "_app/auth" for Auth

var account = Auth.require()
if (account is Map == false) return account  // Auth.require() returned an error response

// ... main logic
```

## Request Handling

```wren
// Query parameters
var id = Request.get("id")
var page = Request.get("page") || "1"
Request.query("id")   // alias for Request.get("id")

// POST form fields
if (Request.isPost) {
  var name = Request.post("name") || ""
}

// JSON body
if (Request.isJson) {
  var data = Request.json()   // == Json.parse(Request.body)
  var name = data["name"]
}

// Headers
var apiKey = Request.header("X-Api-Key")

// HTTP method
Request.method     // "GET", "POST", "PUT", "DELETE", ...
Request.isPost      // true for POST

// Path segments (only inside _route.wren)
var id = Request.route(0)

// Basic auth
if (Request.login("admin", "secret") != false) return
// ... authenticated code below ...
```

> **Pitfall:** `Request.get(name)` never reads the POST body, even when the
> current request is a POST — it only ever reads the URL query string. If a
> value needs to survive both a GET (rendering a form) and the POST that
> submits it, read both explicitly: `Request.get("slug") || Request.post("slug")`.
>
> **Pitfall:** on failed Basic auth, `Request.login` sets a `401` status and
> the `WWW-Authenticate` header but does **not** stop the script by itself
> — always `return` immediately when it's not `false` (as above), or the
> rest of the handler still runs and its output becomes the body of the
> 401 response.

## Response Handling

```wren
Response.json({"success": true, "id": user.id})
Response.status(404)
return Response.redirect("/users")
Response.notFound()
Response.forbidden()

// CORS
if (Response.cors) return                              // allow all origins
if (Response.cors("https://myapp.com")) return          // specific origin
if (Response.cors("https://myapp.com", "GET, POST", "Content-Type")) return
```

`Response.cors` handles OPTIONS preflight automatically (204 response,
returns `true` so you can `return` early); returns `false` for other methods
so processing continues.

## Sessions and Authentication

```wren
// Login
if (Request.isPost) {
  var user = `SELECT * FROM users WHERE email = ?`.first(Request.post("email") || "")
  if (user && Util.verify(Request.post("password") || "", user["password"])) {
    Session.set("userId", user["id"])
    return Response.redirect("/dashboard")
  }
}

// Check logged in (on protected pages)
if (!Session.get("userId")) return Response.redirect("/login")

// Logout
Session.destroy()
return Response.redirect("/")
```

`Session` is also useful to scope data to an anonymous visitor without any
login system — e.g. a per-visitor todo list:

```wren
class Task {
  construct new(data) {
    _id = data["id"]
    _session = data["session"] || Session.id
  }
  save() { _id = `Task`.save(this) }
  static list() { `SELECT * FROM Task WHERE session = ?`.fetch(Session.id).to(Task) }
}
```

CSRF: `Session.csrf` renders a hidden input field with a token; check it with
`Session.csrfOk` when processing the form submission.

## Cron

```wren
// _app/cron.wren
import "/_app/domain" for Task

// Runs when the current minute is divisible by the given value
// (safe values: 1, 2, 3, 4, 5, 6, 10, 12, 15, 20, 30 — anything that
// doesn't divide 60, like 90, never fires)
Cron.every(5) { |date| System.log("👋 Hello, from Cron!") }

// Runs daily at 02:00
Cron.at(2, 0) { |date| Task.clearAll() }

// Runs every Monday at 04:30 (dayOfWeek: 0=Sunday .. 6=Saturday)
Cron.at(4, 30, 1) { |date| System.log("Monday task") }
```

## File Uploads

Files are always stored in the SQLite database, never on disk.

```wren
if (Request.isPost) {
  var file = Request.file("upload_file")
  if (!file) return <p>No file was uploaded.</p>
  return <p>Uploaded: {{ file.name }}</p>
}

return <form method="post" enctype="multipart/form-data">
  <input type="file" name="upload_file" />
  <button type="submit">Upload</button>
</form>
```

Files handled via `Request.file()` are permanent by default. Create files
dynamically with `File.create(name, type, content)`; serve any stored file
with `Response.file(id)`. Unprocessed uploads and files marked with
`file.temp` are deleted automatically after a short period.

## External Imports

Import community Wren modules straight from GitHub — no package manager:

```wren
import "gh:owner/repo/path/to/file" for ClassName             // main branch
import "gh:owner/repo/path/to/file@v1.0" for ClassName as V1  // pinned tag
import "https://raw.githubusercontent.com/owner/repo/main/module.wren" for ClassName
```

The `.wren` extension is auto-appended for `gh:` imports (required for full
URLs). Modules are downloaded once and cached in `BIALET_REMOTE_MODULES` —
they never auto-update, so pin a tag for anything beyond a quick experiment.

**Local project imports without a leading `/`** (`import "_app/domain" for
User`, `import "./helper" for Helper`) are relative to the directory of the
file doing the importing — not to the project root — and that's true at
every level, including files reached transitively. Writing `import
"_app/domain" for User` from a root-level page (`index.wren`) works because
the page's own directory *is* the project root, but the exact same string
written inside a file that already lives under `_app/` would look for
`_app/_app/domain.wren` instead. **Prefer a leading `/`** — `import
"/_app/domain" for User` — for anything in `_app/`; it always resolves from
the project root no matter which file imports it or how deep it is.

## Markdown

```wren
Markdown.html("## Hello **World**!")   // renders to HTML
Markdown.file("about.md")              // reads and renders a file from the app dir
```

## Security Checklist

1. **Never concatenate or interpolate values into SQL** — always use `?`
   placeholders and pass parameters to the query method.
2. **Rely on `{{ }}` auto-escaping** — interpolation escapes plain strings
   (user input, database, URL) automatically. Mark intentionally-raw markup
   with `HtmlNode` or `.raw`.
3. **Always `return` before `Response.redirect(...)`** so the rest of the
   script doesn't attach a body to the redirect.
4. **Validate input** in domain classes (`isValid`, `errors`) or inline
   before saving.
5. **Pin external imports** to a tag, not `main`, for anything beyond local
   experiments.

```wren
// WRONG — SQL injection
`SELECT * FROM users WHERE name = '%(name)'`.fetch

// WRONG — XSS
<p>{{ userInput.safe }}</p>  // double-escaped: interpolation already escapes

// CORRECT
`SELECT * FROM users WHERE name = ?`.fetch(name)
<p>{{ userInput }}</p>
```

## JSON APIs

```wren
// api/users.wren — handles /api/users

if (Response.cors) return

var id = Request.get("id")

// GET /api/users?id=1
if (id && Request.method == "GET") {
  var user = User.find(id)
  if (!user) {
    Response.status(404)
    return Response.json({"error": "User not found"})
  }
  return Response.json({"id": user.id, "name": user.name, "email": user.email})
}

// GET /api/users
if (!id && Request.method == "GET") {
  return Response.json(User.all().map{|u| {"id": u.id, "name": u.name, "email": u.email}}.toList)
}

// POST /api/users
if (Request.method == "POST") {
  var data = Request.json()
  var user = User.new()
  user.name = data["name"]
  user.email = data["email"]

  if (!user.isValid) {
    Response.status(400)
    return Response.json({"error": "Invalid user data"})
  }

  user.save()
  Response.status(201)
  return Response.json({"id": user.id, "name": user.name})
}

Response.status(404)
return Response.json({"error": "Not found"})
```

This single-file pattern — one `.wren` file handling GET/POST/PUT/DELETE via
`Request.method`, with the resource id from a query parameter — is the
idiomatic way to build a REST endpoint in Bialet. Reach for `_route.wren`
only when the id truly needs to live in the URL path.

## HTMX Integration (Optional)

Full page reloads are the default. Add HTMX sparingly, only where it clearly
improves UX (inline delete, edit-in-place):

```wren
<script src="https://unpkg.com/htmx.org@1.9.10"></script>

<button hx-delete="/users/{{ user.id }}" hx-confirm="Are you sure?"
        hx-target="closest tr" hx-swap="outerHTML">Delete</button>
```

## Semantic HTML & Styling

Use proper elements — `<header>`, `<nav>`, `<main class="container">`,
`<article>`, `<section>`, `<aside>`, `<footer>` — and let a classless CSS
framework like [PicoCSS](https://picocss.com) style them with zero classes.
Tailwind (via CDN for prototyping) and vanilla CSS also work — Bialet just
outputs plain HTML, so any CSS approach applies unchanged. Stylesheets go
anywhere except behind a `_`/`.` prefix (those return 403).

## Complete Example: Todo List App

```
todo-app/
├── _app/
│   ├── template.wren
│   ├── migration.wren
│   └── domain.wren
├── .gitignore
├── index.wren        # / — list + add form
├── toggle.wren        # POST /toggle — mark done/undone
├── delete.wren         # POST /delete — remove a task
└── css/
    └── style.css
```

```wren
// _app/migration.wren
Db.migrate("Create tasks table", `
  CREATE TABLE tasks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    description TEXT NOT NULL,
    finished BOOLEAN DEFAULT 0,
    session TEXT,
    createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
  )
`)
```

```wren
// _app/domain.wren
class Task {
  construct new(data) {
    _id = data["id"]
    _description = data["description"] || ""
    _finished = data["finished"] || false
    _session = data["session"] || Session.id
    _createdAt = data["createdAt"]
    if (!_createdAt) _createdAt = Date.new().toString
  }
  static new() { Task.new({}) }

  id { _id }
  description { _description }
  createdAt { _createdAt }
  finished { _finished == "1" || _finished == true }
  description=(val) { _description = val.toString.trim() }

  save() { _id = `tasks`.save(this) }

  static toggle(id) { `UPDATE tasks SET finished = NOT finished WHERE id = ? AND session = ?`.query(id, Session.id) }

  static list() { `SELECT * FROM tasks WHERE session = ? ORDER BY createdAt ASC`.fetch(Session.id).to(Task) }
  static delete(id) { `DELETE FROM tasks WHERE id = ? AND session = ?`.query(id, Session.id) }
}
```

```wren
// _app/template.wren
class Layout {
  static render(content) { <!doctype html>
    <html lang="en">
      <head>
        <meta charset="utf-8" />
        <title>Todo</title>
        <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/@picocss/pico@2/css/pico.min.css" />
      </head>
      <body>
        <main class="container">{{ content }}</main>
      </body>
    </html> }
}
```

```wren
// index.wren
import "_app/template" for Layout
import "_app/domain" for Task

if (Request.isPost) {
  var task = Task.new()
  task.description = Request.post("description") || ""
  task.save()
  return Response.redirect("/")
}

var tasks = Task.list()

return Layout.render(<main>
  <h1>Todo List</h1>
  <form method="post">
    <input name="description" placeholder="What needs to be done?" required />
    <button type="submit">Add</button>
  </form>
  <ul>
    {{ tasks.map{|task| <li>
      <form method="post" action="/toggle" style="display:inline">
        <input type="hidden" name="id" value="{{ task.id }}" />
        <button>{{ task.finished ? "✓" : "○" }}</button>
      </form>
      <span class="{{ task.finished ? "done" : "" }}">{{ task.description }}</span>
      <form method="post" action="/delete" style="display:inline">
        <input type="hidden" name="id" value="{{ task.id }}" />
        <button>✕</button>
      </form>
    </li>} }}
  </ul>
  {{ tasks.isEmpty && <p>No tasks yet. Add your first one above.</p> }}
</main>)
```

```wren
// toggle.wren
import "_app/domain" for Task

var id = Request.post("id")
if (id) Task.toggle(id)
return Response.redirect("/")
```

```wren
// delete.wren
import "_app/domain" for Task

Task.delete(Request.post("id") || "")
return Response.redirect("/")
```

## Running the App

```bash
bialet              # runs the current directory on 127.0.0.1:7001
bialet -p 7001 .    # explicit port and directory
bialet -t index.wren              # validate a standalone file's syntax
bialet -t app/main.wren /my/app   # validate with app context (for _app/ imports)
```

## Summary

1. **Code Style**: 2-space indentation, single-line methods when possible
2. **Structure**: `_app/` folder for template, migrations, and domain classes
3. **No extra folders**: no `routes/`, `validators/`, `middleware/`, `static/`
4. **Routing**: query parameters by default (`Request.get`); `_route.wren` +
   `Request.route(n)` only for path-based URLs, as an exception
5. **Controllers**: each `.wren` file IS a route — logic at the top, view at
   the bottom, always `return Response.redirect(...)`
6. **Models**: domain classes with validation and `.to(Class)` mapping
7. **Database**: backtick SQL, `?` placeholders, `_app/migration.wren` for schema
8. **Security**: parameterized queries always; `{{ }}` auto-escapes untrusted strings
9. **APIs**: `Response.json()`, one file per resource, method + query param
   for the id, rather than per-verb files
10. **Git**: always add `_db.sqlite3*` to `.gitignore`
11. **HTML-first**: reach for `<details>`, `popover`, the checkbox/radio CSS
    hack, and server-side query-param filtering before writing JS
