# FAQ

## General

### What is Bialet?

Bialet is a full-stack web framework that integrates the
[Wren language](https://wren.io) with an HTTP server and a built-in SQLite
database in a single binary. You write HTML with embedded queries and logic
in `.wren` files — no build steps, no configuration files, no separate
database server.

### Why the name "Bialet"?

Bialet is named after [Juan Bialet Massé](https://es.wikipedia.org/wiki/Juan_Bialet_Mass%C3%A9),
an Argentine physician, lawyer, and educator known for his practical,
no-nonsense approach to solving problems.

### Is Bialet production-ready?

Bialet is stable and used in production for small to medium traffic sites.
It's well-suited for CRUD apps, dashboards, internal tools, and prototypes.
If you need horizontal scaling, WebSocket support, or high-throughput
architectures, you may want to evaluate other frameworks.

### How does Bialet compare to PHP / Flask / Rails / Django?

Bialet is closer to PHP in philosophy: files map to URLs, no build step, and
you write server-rendered HTML. Unlike PHP, Bialet bundles SQLite and the
entire runtime in a single binary. Compared to Flask or Rails, Bialet is
simpler and more opinionated — there's no ORM, no middleware stack, and no
routing DSL. The trade-off is less flexibility for less complexity.

### When should I use Bialet?

- Prototyping and internal tools
- Simple CRUD applications
- Learning web development without configuration overhead
- Single-server deployments where you want zero infrastructure

### When should I NOT use Bialet?

- Applications requiring horizontal scaling or load balancing
- Real-time features (WebSockets, SSE) — not currently supported
- Teams that need a mature ecosystem of plugins and libraries
- Projects where you must use PostgreSQL, MySQL, or other databases

### What license is Bialet?

MIT. You can use, modify, and distribute it freely.

## Technical

### Can I use JavaScript?

Yes. JavaScript is welcome but never required. Include `<script>` tags in
your HTML, link to external JS files, or use libraries like
[Alpine.js](https://alpinejs.dev) for interactivity. Bialet apps are
classic multi-page applications — use forms and links for navigation, and
sprinkle JS where you need client-side behavior.

### Can I use other databases besides SQLite?

No. Bialet is built around SQLite. The database file (`_db.sqlite3`) is
created automatically in your app directory. This keeps deployment simple:
copy your files, and the database comes along.

### What platforms are supported?

Pre-built binaries for:
- macOS ARM (Apple Silicon)
- Linux x86_64
- Linux ARM64

You can also build from source on any platform with a C17 compiler, SQLite3,
and libcurl (see [Installation](installation.md)).

### Can I use CSS frameworks?

Yes. Link any CSS file in your `_app.wren` template or include a `<link>` in
your HTML. The Getting Started tutorial uses
[Flowbite](https://flowbite.com/) + [Tailwind](https://tailwindcss.com/),
but [Pico CSS](https://picocss.com), Bootstrap, or plain CSS work fine.

### How does routing work?

Bialet uses file-based routing. A request to `/about` maps to
`about.wren` (or `about.html`). For dynamic routes, create a `_route.wren`
file in a directory and use `Request.route(pos)` to read URL segments. See
[Structure & Routing](structure.md).

### How do I handle forms?

Use `Request.isPost` to check the method and `Request.post("field")` to read
submitted values. Redirect after processing with
`Response.redirect("/path")`. See the [Getting Started](getting-started/index) tutorial.
tutorial for a complete example.

### How do database queries work?

Use backtick strings for prepared queries:

```wren
var users = `SELECT id, name FROM users WHERE active = ?`.fetch(1)
```

Query methods: `.fetch()`, `.first()`, `.val()`, `.toNum()`, `.toBool()`.
See [Database](database.md) for details.

### DB values come back as strings. How do I work with numbers?

Use `query.toNum()` for direct numeric retrieval, or `Num.fromString()`
for manual conversion:

```wren
var count = `SELECT COUNT(*) as c FROM votes`.toNum
var age = Num.fromString(row["age"])
```

### How do I create reusable layouts?

Create an `_app.wren` file with a `Template` class. This file is
automatically loaded and its class is available to all pages in your app.
Use the `Site` variable to access it:

```wren
// _app.wren
class Template {
  static head() { return "<head>...</head>" }
  static body() { return "</body></html>" }
}
```

```wren
// index.wren
Site.head()
return "<h1>Hello</h1>"
Site.body()
```

### Can I import external Wren modules?

Yes, using `import` with a GitHub path:

```wren
import "gh:bialet/extra/mcp" for Mcp
```

See [Structure & Routing](structure.md).

### Does Bialet support HTTPS?

Bialet itself serves HTTP. For HTTPS in production, put a reverse proxy
like nginx or Caddy in front of it. For local development, HTTP on
`localhost` is fine.

### How do I deploy a Bialet app?

Copy your app directory and the `bialet` binary to a server, then run:

```bash
bialet /path/to/app
```

That's it. No build pipeline, no CI/CD required. Use Docker Compose
(see [Installation](installation.md)) if you prefer containers.

### Can I run multiple apps on one server?

Yes. Run `bialet` with different `-p` (port) and `-A` (app directory)
flags:

```bash
bialet -A /apps/site1 -p 7001 &
bialet -A /apps/site2 -p 7002 &
```

### Does Bialet support WebSockets?

Not currently. Bialet is designed for request/response HTTP. For real-time
features, consider polling or integrating an external WebSocket service.

### Is there a live-reload / hot-reload feature?

On Linux, Bialet uses inotify to watch `.wren` files and automatically
reload when they change. On macOS, restart the server manually for now.

## Community

### How do I report a bug?

Open an issue on [GitHub Issues](https://github.com/bialet/bialet/issues).

### How can I contribute?

See the [Contributing Guide](https://github.com/bialet/bialet/blob/main/CONTRIBUTING.md)
for setup, coding guidelines, and pull request workflow.

### What's planned for future releases?

See the [Roadmap](https://github.com/bialet/bialet/blob/main/ROADMAP.md) for
planned features and improvements.

### Where can I ask questions?

- [GitHub Issues](https://github.com/bialet/bialet/issues) — for bugs and
  feature requests
- [GitHub Discussions](https://github.com/bialet/bialet/discussions) —
  for questions and community chat (check if enabled)

### Is there a way to try Bialet online?

Not yet, but you can install and run it in seconds:

```bash
curl -sSL https://get.bialet.dev | sh
echo 'return <h1>Hello!</h1>' > index.wren
bialet
```
