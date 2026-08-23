# Advanced Routing

Bialet routing is exactly like serving static HTML files. A file named
`about.wren` is the route `/about`, just as `about.html` would be. The
`.wren` extension is optional in the URL.

**The primary way to pass dynamic data is via query strings, not path
segments.** A file named `article.wren` at `/article` can read `?id=42`
with `Request.get("id")` — no `_route.wren` needed. This keeps your
project purely file-based, just like static HTML: one file, one URL, any
number of query-string variations.

If you've ever put an HTML file in a folder and served it with a static
file server, you already understand Bialet routing. There is no router to
configure, no `app.get(...)` calls, no `urlpatterns` list. You put a file
where you want a URL, and the URL exists.

## Quick Start

Here's the entire mental model in one directory tree:

```text
my-site/
├── index.wren          # /
├── about.wren          # /about
└── article.wren        # /article  (dynamic via ?id=...)
```

**The rule:** a file's path relative to the project root, minus the `.wren`
extension, is its URL.

- `index.wren` at the root of a folder serves that folder's URL (`/`,
  `/blog`).
- `about.wren` serves `/about` or `/about.wren` — both work.
- `article.wren` serves `/article` — and reads whatever comes after the
  `?` to decide what to render.

```wren
// File: article.wren
// Handles: /article?id=42

var id = Request.get("id")
var article = `SELECT title, body_html FROM articles WHERE id = ?`.first([id])

if (!article) return Response.notFound()

return <main>
    <h1>{{ article["title"]}}
    {{ article["body_html"] }}
</main>
```

No new file, no special name, no `_route.wren` — just a normal `.wren`
file reading `Request.get("id")`, the same way a static-page backend
would read `$_GET['id']`.

**Mental model:** Bialet resolves URLs by walking the file system. You
never declare routes explicitly — the layout of your project *is* the
routing table. Dynamic *data* comes from the query string; dynamic
*routes* are a rare, separate case covered later in
[Path-Based Dynamic Routes (Advanced)](#path-based-dynamic-routes-advanced).

> ⚠️ Pitfall: `/about.wren` and `/about` both serve `about.wren`, but the
> `.wren` extension is never required in links you write. Always link to
> the extension-less form.

> **Note:** if you truly need the dynamic value *in the path* —
> `/article/42` instead of `/article?id=42` — Bialet supports that too,
> via a special `_route.wren` file. Treat it as an advanced escape hatch,
> not your starting point; see
> [Path-Based Dynamic Routes (Advanced)](#path-based-dynamic-routes-advanced).

The rest of this page fills in the details: direct mapping, protected
files, dynamic content via query parameters, path-based routes (advanced),
a full example project, and external imports.

## Direct File Mapping

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
doesn't. For dynamic data, start with
[Dynamic Content with Query Parameters](#dynamic-content-with-query-parameters)
below — reach for
[Path-Based Dynamic Routes](#path-based-dynamic-routes-advanced) only if
the value truly needs to live in the URL path.

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
_route.wren      # Dynamic route handler (advanced, see below)
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

This is the default way to make a page dynamic in Bialet — reach for it
first, every time. Any `.wren` file, with no special name and no extra
file, can read query parameters with `Request.get(name)`. It's the same
model a static HTML page would use if it handed off to a server-side
script reading `$_GET` in PHP, or `req.query` in Express — except here,
every `.wren` file already has that ability, by default.

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

> ⚠️ Pitfall: don't reach for `_route.wren` just because you have a single
> dynamic page. Query parameters are simpler, more cache-friendly, and
> keep your project structure flat. Use path segments only when the URL
> truly represents a resource hierarchy (like a REST API) or you need the
> SEO benefit of human-readable slugs.

(path-based-dynamic-routes-advanced)=

## Path-Based Dynamic Routes (Advanced)

> **When to use this:** only when the dynamic value must live in the URL
> *path* itself — a human-readable slug for SEO (`/blog/how-to-cook-rice`),
> a REST-style resource path (`/api/users/123`), or because you're
> preserving a URL structure you inherited and can't change. If none of
> those apply, use
> [query parameters](#dynamic-content-with-query-parameters) instead — see
> the pitfall above. This is an escape hatch, not a starting point, and
> most Bialet projects never need it.

Fixed files can't cover URLs with variable path segments. For those rare
cases, create a `_route.wren` file in the relevant directory. Bialet
falls back to it whenever no static file matches the requested path.

```wren
// File: api/_route.wren
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

> ⚠️ Pitfall: dynamic segments start at index `0`, not `1`. `Request.route(0)`
> is the first segment after the `_route.wren` file's own directory.

| URL | `Request.route(0)` | `Request.route(1)` | `Request.get("fields")` |
| --- | --- | --- | --- |
| `/api/users/1` | `"users"` | `"1"` | `null` |
| `/api/posts/hello-world` | `"posts"` | `"hello-world"` | `null` |
| `/api/users/1?fields=name,email` | `"users"` | `"1"` | `"name,email"` |

Each segment is captured as a single path component — this is not a regex
router. `Request.route(n)` never matches across a `/`. Query parameters
still work alongside path segments, as the `fields` column above shows —
the two aren't mutually exclusive, you're just choosing where the primary
identifier lives.

You don't need one global `_route.wren` for the whole site. Create a
separate `_route.wren` in each directory that needs path-based handling —
`blog/_route.wren`, `admin/posts/_route.wren`, `api/_route.wren` can all
coexist and handle their own subtree independently.

### How a Request Is Resolved

```text
                 ┌────────────────────┐
                 │  Incoming request  │
                 └──────────┬─────────┘
                            │
                            v
              ┌─────────────────────────────┐
              │ Matching .wren file exists?  │
              └───────┬─────────────┬───────┘
                   yes │             │ no
                       v             v
              ┌────────────────┐   ┌───────────────────────────────┐
              │ Execute that   │   │ _route.wren exists in this    │
              │ file           │   │ directory?                    │
              └────────────────┘   └───────┬───────────────┬───────┘
                                        yes │               │ no
                                            v               v
                              ┌─────────────────────────┐ ┌──────────────┐
                              │ Execute _route.wren with │ │ 404 Not Found│
                              │ Request.route(n) segments│ └──────────────┘
                              └─────────────────────────┘
```

Protected paths (`_`/`.` prefixed) short-circuit this diagram entirely —
they return 403 before file resolution is attempted.

## Complete Example Project Structure

A blog application using the `_app/` folder approach, with query
parameters as the default dynamic-content strategy:

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
│   └── posts/
│       ├── index.wren     # Post list (/admin/posts)
│       ├── new.wren       # Create post (/admin/posts/new)
│       └── _route.wren    # Edit post (/admin/posts/:id)
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
> with `_route.wren` for path-based URLs — `blog/_route.wren` for
> `/blog/my-first-post`, `api/_route.wren` for `/api/posts/456`. Only do
> that if you need the slug or a REST-style path; see
> [Path-Based Dynamic Routes (Advanced)](#path-based-dynamic-routes-advanced).
> The structure above uses query parameters instead, which is simpler for
> most projects.

| URL | File Executed | Purpose |
| --- | --- | --- |
| `/` | `index.wren` | Homepage |
| `/about` | `about.wren` | About page |
| `/blog` | `blog/index.wren` | Blog post list |
| `/article?id=42` | `article.wren` | Single post, looked up by `?id=` |
| `/admin` | `admin/index.wren` | Admin dashboard |
| `/admin/posts/new` | `admin/posts/new.wren` | Create new post form |
| `/admin/posts/123` | `admin/posts/_route.wren` | Edit post with ID 123 |
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

Only do this if you need `/blog/my-first-post` in the address bar instead
of `/article?id=42` — typically for SEO, or because you're matching a URL
structure you don't control. It requires a `slug` column and an extra
file; most projects don't need it, and the query-parameter version above
should be your default.

**File:** `blog/_route.wren`

```wren
import "_app/template" for Template

var slug = Request.route(0)

if (!slug) {
  Response.redirect("/blog")
  return
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

> ⚠️ Pitfall: this version needs a dedicated `slug` column and gives up
> the simplicity of a single query-string-driven file. Only add it if the
> SEO or URL-structure requirement is real — see
> [Path-Based Dynamic Routes (Advanced)](#path-based-dynamic-routes-advanced).

## Routing Compared: Bialet vs Express vs Django

The same feature — fetching one blog post dynamically — looks different
depending on whether routing is file-based or code-based, and on whether
the dynamic value lives in a query string or the path. Bialet's default is
the query string; Express and Django default to the path.

**Bialet — idiomatic: query parameter, no route table:**

```wren
// File: blog.wren
var id = Request.get("id")
var post = `SELECT * FROM posts WHERE id = ?`.first([id])
return post
```

URL: `/blog?id=42`. The route exists because the file exists — there's no
separate list of routes to keep in sync with your handlers, and no
special file needed for this, the common case.

**Express — code-based, explicit route table, path parameter:**

```js
app.get('/blog/:slug', (req, res) => {
  const post = db.get('SELECT * FROM posts WHERE slug = ?', req.params.slug)
  res.send(post)
})
```

You register the path pattern and handler together, usually in a routes
file that grows as the app grows.

**Django — code-based, `urlpatterns` + views, path parameter:**

```python
# urls.py
urlpatterns = [
    path('blog/<slug:slug>/', views.post_detail),
]

# views.py
def post_detail(request, slug):
    post = Post.objects.get(slug=slug)
    return render(request, 'post.html', {'post': post})
```

Django separates the URL pattern (`urls.py`) from the view function
(`views.py`) — two files to keep aligned for every route.

| | Bialet (idiomatic) | Bialet (path-based, advanced) | Express | Django |
| --- | --- | --- | --- | --- |
| Route declared by | file path | file path + `_route.wren` | `app.get(...)` call | `urlpatterns` entry |
| Dynamic value from | `Request.get("id")` | `Request.route(n)` | `req.params.slug` | `<type:name>` in path |
| Handler location | same file as the route | same file as the route | inline callback | separate `views.py` |
| Adding a route | add a file | add a `_route.wren` file | add a line to a routes file | add a `path()` and a view |
| Typical use | default — most pages | rare — SEO slugs, REST APIs | default | default |

The path-based Bialet row exists for parity with how Express and Django
work by default. In Bialet it's the exception, not the rule — most
projects never write a `_route.wren` file.

If you're coming from Express or Django, the biggest mental shift isn't
file-based vs. code-based routing — it's that the dynamic value usually
comes from the query string, not the path. Reach for `Request.get(...)`
by default; reach for `Request.route(n)` only when you have a concrete
reason to.

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

- **Query parameters are the default** — any `.wren` file reads dynamic
  data with `Request.get(name)`; no special file or extra route needed.
- **File-based routing** — every `.wren` file is a route, exactly like a
  static HTML file.
- **Protected files** — anything starting with `_` or `.` is inaccessible
  directly and returns 403.
- **Path-based routes are the exception** — `_route.wren` plus
  `Request.route(n)` exists for SEO slugs and REST-style APIs; reach for
  it rarely, and only with a concrete reason.
- **No route table** — the file system is the routing table. Nothing to
  register, nothing to keep in sync.
- **External imports** — `gh:` shorthand or full URLs, cached in
  `BIALET_REMOTE_MODULES`, never auto-updating once cached. See
  [External Modules](external-modules.md) for details.
