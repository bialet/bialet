# Advanced Routing

Bialet routing is exactly like serving static HTML files. A file named
`about.wren` is the route `/about`, just as `about.html` would be. The
`.wren` extension is optional in the URL.

Dynamic data comes from two equally simple places: **query strings**
(`article.wren` reading `?id=42` with `Request.get("id")`) or **path
segments** (`blog.wren` reading `/blog/my-post` with `Request.route(0)`).
Both are first-class — pick whichever fits the URL you want. What Bialet
discourages is *complex* routing: deep nesting and convoluted URL
hierarchies. Keep it simple.

There is no router to configure, no `app.get(...)` calls, no `urlpatterns`
list. **The layout of your project *is* the routing table** — you put a file
where you want a URL, and the URL exists.

## Overview

Here's the entire mental model in one directory tree:

```text
my-site/
├── index.wren          # /
├── about.wren          # /about
└── article.wren        # /article/my-article  (dynamic via path slug)
```

**The rule:** a file's path relative to the project root, minus the `.wren`
extension, is its URL.

- `index.wren` at the root of a folder serves that folder's URL (`/`,
  `/blog`).
- `about.wren` serves `/about` or `/about.wren` — both work.
- `article.wren` serves `/article` — and reads whatever follows it in the
  path to decide what to render.

```wren
// File: article.wren
// Handles: /article/my-article

var slug = Request.route(0)
var article = `SELECT title, body_html FROM articles WHERE slug = ?`.first(slug)

if (!article) return Response.notFound()

return <main>
    <h1>{{ article["title"]}}
    {{ article["body_html"].raw }}
</main>
```

No new file, no special name — the same file serves `/article` and every
`/article/<slug>` URL via `Request.route(0)`.

> ⚠️ Pitfall: `/about.wren` and `/about` both serve `about.wren`, but the
> `.wren` extension is never required in links you write. Always link to
> the extension-less form.

> **Both styles are equal.** A dynamic value can live in the query string
> (`/article?id=42`) or in the path (`/article/my-post` via a
> `<folder>.wren` file) — Bialet treats them the same. Pick whichever
> produces the URL you want; the path form is covered in
> [Path-Based Dynamic Routes](#path-based-dynamic-routes-advanced).

### Direct File Mapping

Every `.wren` file maps to a URL path the same way a static HTML file
would.

```text
contact-us.wren  →  /contact-us
                 →  /contact-us.wren   (extension optional)
```

```text
landing/newsletter/cool-campaign.wren  →  /landing/newsletter/cool-campaign
```

| File | URL |
| --- | --- |
| `index.wren` | `/` |
| `contact-us.wren` | `/contact-us` and `/contact-us.wren` |
| `landing/newsletter/cool-campaign.wren` | `/landing/newsletter/cool-campaign` |
| `blog/index.wren` | `/blog` |

This is not a regex router. There's no pattern matching, no wildcards, no
route priority to reason about. A file either exists at that path or it
doesn't. For dynamic data, see
[Dynamic Content with Query Parameters](#dynamic-content-with-query-parameters)
and [Path-Based Dynamic Routes](#path-based-dynamic-routes-advanced)
below — pick whichever gives you the URL you want, and keep it shallow.

## How a Request Is Resolved

When a request arrives, Bialet resolves the URL in this order:

1. **Exact file match** – if `<full_path>.wren` exists, execute it.
2. **Folder-file walk-up** – walk up the directory tree, checking each
   parent folder for a `<folder>.wren` file. The **deepest** match (closest
   to the URL) wins.
3. **404 Not Found** – if no file is found.

Example: `/some/lorem/ipsum`

- If `/some/lorem/ipsum.wren` exists → execute it (no dynamic segments).
- Else if `/some/lorem.wren` exists → execute it; `Request.route(0)` =
  `"ipsum"`.
- Else if `/some.wren` exists → execute it; `Request.route(0)` = `"lorem"`,
  `Request.route(1)` = `"ipsum"`.
- Else → 404.

> Protected folders (`_` or `.`) are never considered during the walk-up —
> they return 403 immediately.

## Protected Files

Files and folders starting with `_` or `.` are **protected**. They cannot
be accessed directly via URL, no matter what extension they have.

> ⚠️ Pitfall: protected files return **403 Forbidden**, not 404. If you get
> a 403 on a path you expected to be a 404, check whether a parent folder
> starts with `_` or `.`.

### Preferred: the `_app/` folder

Keep application-wide files inside a single protected folder:

```text
_app/template.wren    # Application-wide template (header, footer, nav)
_app/migration.wren   # Database migrations
_app/cron.wren        # Scheduled tasks
_app/domain.wren      # Domain-specific configuration (optional)
_db.sqlite3           # SQLite database file
```

> **Configuration note:** Bialet doesn't use `.env` files. Configuration
> lives in the `BIALET_CONFIG` table inside your SQLite database
> (`_db.sqlite3`). Each environment has its own database file, so
> configuration is environment-specific by construction. See the
> [Config class reference](reference.md) for managing configuration
> values.

### Alternative: root-level special files

You can place special files directly at the root instead, each prefixed
with `_`:

```text
_app.wren        # Application-wide template
_migration.wren  # Database migrations
_cron.wren       # Scheduled tasks
```

Both approaches work identically. The `_app/` folder keeps your root
directory cleaner; use root-level files if you prefer fewer nested
folders.

### Ignored files

Some files are ignored entirely and are never served, protected or not:
`README*`, `AGENTS*`, `LICENSE*`, `*.json`, `*.yml`, `*.yaml`. Keep
documentation, AI agent instructions, and config files in your project
without worrying about them leaking through routing.

(dynamic-content-with-query-parameters)=

## Dynamic Content with Query Parameters

This is one of the two simple ways to make a page dynamic in Bialet. Any
`.wren` file, with no special name and no extra file, can read query
parameters with `Request.get(name)`. It's the same model a static HTML
page would use if it handed off to a server-side script reading `$_GET` in
PHP, or `req.query` in Express — except here, every `.wren` file already
has that ability.

```wren
// File: blog.wren
// Handles: /blog?id=42

import "_app/template" for Template

var id = Request.get("id")
var post = `
  SELECT title, content, createdAt, author
  FROM posts
  WHERE id = ? AND published = 1
`.query(id).fetch()

if (!post) {
  Response.status(404)
  return "<h1>Post not found</h1>"
}

return Template.new().layout(<article>
  <h1>{{post["title"]}}</h1>
  <p class="meta">By {{post["author"]}} on {{post["createdAt"]}}</p>
  <div class="content">{{post["content"]}}</div>
</article>)
```

One file (`blog.wren`) now serves every post on the site — `/blog?id=1`,
`/blog?id=2`, `/blog?id=9001` — with no additional files and no routing
configuration to maintain.

The same pattern composes for anything you'd otherwise be tempted to put
in the path, including simple API-style endpoints:

```wren
// File: api.wren
// Handles: /api?action=users&id=1  or  /api?action=posts&id=42

var action = Request.get("action")
var id = Request.get("id")

if (action == "users") {
  var userId = Num.fromString(id)
  var user = `SELECT * FROM users WHERE id = ?`.first([userId])
  return user

} else if (action == "posts") {
  var post = `SELECT * FROM posts WHERE id = ?`.first([Num.fromString(id)])
  return post
}
```

| URL | `Request.get("action")` | `Request.get("id")` | `Request.get("fields")` |
| --- | --- | --- | --- |
| `/api?action=users&id=1` | `"users"` | `"1"` | `null` |
| `/api?action=posts&id=42` | `"posts"` | `"42"` | `null` |
| `/api?action=users&id=1&fields=name,email` | `"users"` | `"1"` | `"name,email"` |

> ⚠️ Pitfall: don't over-engineer either style. A single dynamic value can
> live in a query string or a path segment — both are equally fine. What
> Bialet discourages is *complex* routing: deep nesting, many segments, or
> elaborate URL hierarchies. Keep dynamic URLs one or two levels deep.

(path-based-dynamic-routes-advanced)=

## Path-Based Dynamic Routes

> Path-based routing is a first-class option, equal to query parameters.
> Use it when the dynamic value reads naturally as part of the path —
> human-readable slugs (`/blog/how-to-cook-rice`), REST-style resources
> (`/api/users/123`), or a URL structure you inherited. Like query
> strings, keep it shallow — see
> [How Deep Should Your Dynamic Routes Go?](#how-deep-should-your-dynamic-routes-go).

### Query string or path segment?

Both are simple, first-class options — pick by how the URL should read:

- **Query string (`Request.get`)** – dynamic data that feels like a
  variable: `/article?id=42`, `/search?q=hello`, `/api/users?id=7`.
- **Path segment (`<folder>.wren` + `Request.route(n)`)** – dynamic data
  that reads as part of the URL itself: `/blog/my-article`,
  `/api/users/123`.

What Bialet discourages is *complex* routing: deeply nested paths, many
segments, or elaborate URL schemes. Keep either style shallow — see
[How Deep Should Your Dynamic Routes Go?](#how-deep-should-your-dynamic-routes-go).

### How it works

Fixed files can't cover URLs with variable path segments. For those URLs,
name a `.wren` file after a folder: `<folder>.wren`. When a URL
doesn't resolve to a static file, Bialet walks up the path looking for a
`.wren` file named after each segment, deepest first. `/api/users/123`
matches `api.wren`, `/blog/how-to-cook-rice` matches `blog.wren`, and a
file that doesn't exist gets a 404.

```wren
// File: api.wren
// Handles URLs like: /api/users/123 or /api/posts/my-slug

var segment = Request.route(0)  // First dynamic segment
var id = Request.route(1)       // Second dynamic segment

if (segment == "users") {
  var userId = Num.fromString(id)
  var user = `SELECT * FROM users WHERE id = ?`.first([userId])
  return user

} else if (segment == "posts") {
  var slug = id
  var post = `SELECT * FROM posts WHERE slug = ?`.first([slug])
  return post
}
```

> ⚠️ Pitfall: dynamic segments start at index `0`, not `1`.
> `Request.route(0)` is the first segment after the `<folder>.wren` file's
> own URL. At the bare folder URL itself (`/api`), `Request.route(0)` is
> `null`.

| URL | Matching file (if any) | `route(0)` | `route(1)` | `route(2)` |
|-----|------------------------|------------|------------|------------|
| `/some` | `some.wren` | `null` | `null` | `null` |
| `/some/lorem` | `some.wren` | `"lorem"` | `null` | `null` |
| `/some/lorem/ipsum` | `some.wren` | `"lorem"` | `"ipsum"` | `null` |
| `/some/lorem/ipsum` (if `lorem.wren` exists) | `some/lorem.wren` | `"ipsum"` | `null` | `null` |
| `/some/lorem/ipsum` (if `ipsum.wren` exists) | `some/lorem/ipsum.wren` | *not used* (exact match) | *not used* | *not used* |

> - `Request.route(0)` is the **first** segment after the `<folder>.wren`
>   file's own URL.
> - If the request matches an exact file, `Request.route(n)` always returns
>   `null`.

A single `<folder>.wren` therefore covers a whole resource: the bare URL
(`Request.route(0)` is `null`) renders the list, and every deeper path
renders an item. This is the classic REST-style list + detail pattern in
one file.

Each segment is captured as a single path component — this is not a regex
router. `Request.route(n)` never matches across a `/`. Query parameters
still work alongside path segments, so the two aren't mutually exclusive —
you're just choosing where the primary identifier lives.

You don't need one global route file for the whole site. Create a
separate `<folder>.wren` for each folder that needs path-based handling —
`blog.wren`, `admin/posts.wren`, `api.wren` can all coexist and handle
their own subtree independently. The deepest match wins, so `admin.wren`
and `admin/posts.wren` can coexist: `/admin/posts/456` runs
`admin/posts.wren`, anything else under `/admin` runs `admin.wren`.

When a folder has both an `index.wren` and a `<folder>.wren`, the
`<folder>.wren` wins for the folder URL itself — Bialet probes the `.wren`
file before the directory index.

## Complete Example Project Structure

A blog application using the `_app/` folder approach. Query parameters are
the dynamic-content strategy here, but path-based routes would work just
as well:

```text
my-blog/
├── _app/                  # Protected folder for app configuration
│   ├── template.wren      # Site-wide template (header, footer, nav)
│   ├── migration.wren     # Database schema setup
│   ├── cron.wren          # Scheduled tasks (optional)
│   └── domain.wren        # Domain config (optional)
│
├── _db.sqlite3            # SQLite database
│
├── index.wren             # Homepage (/)
├── about.wren             # About page (/about)
├── contact.wren           # Contact page (/contact)
├── article.wren           # Single post, by ?id= (/article?id=42)
│
├── blog/
│   └── index.wren         # Blog list (/blog)
│
├── admin/
│   ├── index.wren         # Admin dashboard (/admin)
│   ├── login.wren         # Admin login (/admin/login)
│   └── posts.wren         # Post list + edit (/admin/posts, /admin/posts/:id)
│
├── api.wren                # API endpoint, by ?action=&id= (/api?action=posts&id=42)
│
├── css/
│   └── style.css          # Static CSS
│
└── js/
    └── main.js            # Static JavaScript
```

> **Optional alternative:** both `blog` and `api` could instead be built
> with `<folder>.wren` for path-based URLs — `blog.wren` for
> `/blog/my-first-post`, `api.wren` for `/api/posts/456`. The choice is
> yours; see
> [Path-Based Dynamic Routes](#path-based-dynamic-routes-advanced).

| URL | File Executed | Purpose |
| --- | --- | --- |
| `/` | `index.wren` | Homepage |
| `/about` | `about.wren` | About page |
| `/blog` | `blog/index.wren` | Blog post list |
| `/article?id=42` | `article.wren` | Single post, looked up by `?id=` |
| `/admin` | `admin/index.wren` | Admin dashboard |
| `/admin/posts` | `admin/posts.wren` | Post list |
| `/admin/posts/123` | `admin/posts.wren` | Edit post with ID 123 |
| `/api?action=posts&id=456` | `api.wren` | API endpoint for post 456, via query params |
| `/_app` | ❌ **403 Forbidden** | Protected file |

### Example: Article Page (Query Parameters)

**File:** `article.wren`

```wren
// Import shared layout from _app folder
import "_app/template" for Template

var id = Request.get("id")

if (!id) {
  Response.redirect("/blog")
  return
}

// Fetch post from database
var post = `
  SELECT title, content, createdAt, author
  FROM posts
  WHERE id = ? AND published = 1
`.query(id).fetch()

if (!post) {
  Response.status(404)
  return "<h1>Post not found</h1>"
}

// Render using the shared template
return Template.new().layout(<article>
  <h1>{{post["title"]}}</h1>
  <p class="meta">By {{post["author"]}} on {{post["createdAt"]}}</p>
  <div class="content">
    {{post["content"]}}
  </div>
</article>)
```

URL: `/article?id=42`

> **Note:** if you're using the root-level structure instead of `_app/`,
> import from `_app` directly: `import "_app" for Template`

> ⚠️ Pitfall: `post["content"]` is trusted content you control (it came
> from your own database), but if you ever interpolate user-submitted
> text into HTML this way, mark it safe with `HtmlNode` or `.raw` only
> if you trust it — `{{ }}` escapes plain strings automatically. Never
> build SQL by string-concatenating request input — use parameterized
> queries (`?` placeholders) as shown above to avoid SQL injection.

### Optional: The Same Page as a Path-Based Slug

Use this when `/blog/my-first-post` reads better in the address bar than
`/article?id=42`. It requires a `slug` column and a `<folder>.wren` file —
a small cost, and the choice is yours: pick whichever URL you prefer.

**File:** `blog.wren`

```wren
import "/_app/template" for Template

var slug = Request.route(0)

if (!slug) {
  var posts = `SELECT title, slug FROM posts WHERE published = 1 ORDER BY createdAt DESC`.fetch
  return Template.new().layout(<main>
    <h1>Blog</h1>
    <ul>{{ posts.map{|p| <li><a href="/blog/{{ p["slug"] }}">{{ p["title"] }}</a></li>} }}</ul>
  </main>)
}

var post = `
  SELECT title, content, createdAt, author
  FROM posts
  WHERE slug = ? AND published = 1
`.query(slug).fetch()

if (!post) {
  Response.status(404)
  return "<h1>Post not found</h1>"
}

return Template.new().layout(<article>
  <h1>{{post["title"]}}</h1>
  <p class="meta">By {{post["author"]}} on {{post["createdAt"]}}</p>
  <div class="content">
    {{post["content"]}}
  </div>
</article>)
```

`blog.wren` now serves both `/blog` (the list, when `Request.route(0)` is
`null`) and `/blog/my-first-post` (a single post).

> ⚠️ Pitfall: this version needs a dedicated `slug` column. Trade it for a
> path-based URL only if the URL shape matters to you — see
> [Path-Based Dynamic Routes](#path-based-dynamic-routes-advanced).

(how-deep-should-your-dynamic-routes-go)=

## How Deep Should Your Dynamic Routes Go?

Bialet supports **any depth** – the filesystem is the only limit. However,
for practical maintainability:

- **1–2 levels** – covers 90% of use cases (e.g., `/category/<slug>`,
  `/api/<resource>/<id>`).
- **3 levels** – rare but acceptable (e.g.,
  `/api/<version>/<resource>/<id>`).
- **4+ levels** – usually a sign that you're over-encoding data in the
  path. Consider using query parameters instead.

> There is **no performance penalty** for deeper routes – the walk-up is a
> few filesystem checks, negligible compared to rendering or database
> queries. But deep URLs are harder for users and search engines, and
> often indicate a design smell.

## Common Pitfalls

- **`route(0)` is not the first segment of the URL** – it's the first
  segment *after* the `<folder>.wren` file's own URL. For `/api/users/123`
  handled by `api.wren`, `route(0)` is `"users"`, not `"api"`.
- **If both `some/index.wren` and `some.wren` exist** – the `<folder>.wren`
  (`some.wren`) wins for `/some`. This is because Bialet checks for `.wren`
  files before directory indexes.
- **Protected folders are never matched** – a `<folder>.wren` inside
  `_app/` will never be reached because `_app/` is blocked entirely.
- **Exact matches always win** – if `/blog/my-post.wren` exists, it will be
  executed for `/blog/my-post` even if `blog.wren` also exists. The walk-up
  stops at the exact match.
- **Query parameters are still available** – you can use both
  `Request.get(...)` and `Request.route(n)` in the same file.

## Routing Compared: Bialet vs Express vs Django

| Framework | Route declared by | Dynamic value from | Typical default |
|-----------|-------------------|-------------------|-----------------|
| Bialet (query string) | file path | `Request.get("id")` | query string |
| Bialet (path-based) | file path + `<folder>.wren` | `Request.route(n)` | path segment |
| Express | `app.get(...)` | `req.params.slug` | path segment |
| Django | `urlpatterns` + view | `<type:name>` in path | path segment |

**Key insight:** Bialet treats query strings and path segments as equal —
pick whichever reads best in the URL. Express and Django put dynamic
values in the path by default; Bialet leaves the choice to you, and
discourages complex routing either way.

## External Imports

Bialet supports importing external Wren modules from remote sources —
`gh:owner/repo/path` shorthand or a full URL — so you can use
community-created libraries without manually downloading and managing
them:

```wren
import "gh:4lb0/emoji/emoji@1.0" for Emoji
```

See [External Modules](external-modules.md) for the full import syntax,
how the download/cache cycle works, how to author and publish your own
module (including the relative-import restriction inside remote code),
and how to clear or programmatically refresh the `BIALET_REMOTE_MODULES`
cache.

## Key Takeaways

- **Simple routing either way** – query strings (`Request.get`) and path
  segments (`<folder>.wren` + `Request.route(n)`) are equal, first-class
  options. Pick whichever reads best in the URL.
- **File-based routing** – every `.wren` file is a route; the filesystem
  *is* the routing table.
- **Protected files (`_` or `.`)** – never served directly, and never
  considered in the walk-up.
- **Complex routing is discouraged** – keep dynamic URLs 1–2 levels deep;
  deep nesting and elaborate schemes are a design smell.
- **No route table** – nothing to register, nothing to keep in sync.
- **External imports** – use `gh:` or full URLs for remote modules (cached
  locally).
