# Template

Bialet renders pages using Inline HTML Strings — HTML written directly inside
your Wren `.wren` files. No template engine, no separate files, no build step.
Write a `.wren` file, return an HTML string, and that's your page.

---

## View: Inline HTML Strings

Inline HTML Strings are delimited by angle brackets `<` and `>`. The string
must begin and end with the same tag. Tag names must be lowercase, start with a
letter, and contain only letters, numbers, and hyphens (hyphens enable custom
elements like `<my-element>`).

```wren
// A simple HTML string assigned to a variable
var str = <p>Hello World</p>
```

### Interpolation

Use `{{ }}` to embed any Wren expression inside HTML. The expression is
evaluated and its result is inserted into the string.

```wren
var name = "John"
var greeting = <h1>Hello, {{ name }}!</h1>
// Output: <h1>Hello, John!</h1>

var sum = <p>{{ 5 + 3 }}</p>
// Output: <p>8</p>
```

Interpolation **escapes plain values automatically** — strings, numbers, and
other non-HTML values have `&`, `<`, `>`, `"`, and `'` replaced with their
HTML entities. HTML string literals, nested `{{ }}` results, `.raw` strings,
and `HtmlNode` values are already marked safe and are inserted verbatim. See
[Security](security.md) for the full rules and the escape pitfalls.

### `return` sends the response

Inside a `.wren` file, `return` sends the value as the HTTP response body
and **stops execution immediately**. Nothing after `return` runs. This is how
every page in Bialet works — the last statement is a `return` with the HTML.

```wren
// index.wren
return <html>
  <head><title>My Page</title></head>
  <body>
    <h1>Hello World</h1>
  </body>
</html>
```

### Attributes

Attributes follow the same rules as regular HTML — lowercase names, quoted
values. You can interpolate values with `{{ }}`.

```wren
var url = "/about"
var label = "Learn more"
var link = <a href="{{ url }}" class="nav-link">{{ label }}</a>
```

### Self-Closing Tags

Certain HTML tags are self-closing. In Bialet, these must include a space
before the closing slash. The final output omits the slash.

- Correct: `<hr />`, `<br />`, `<input value="{{ val }}" />`, `<meta charset="utf-8" />`
- Incorrect: `<hr/>`, `<br/>`

```wren
var inputField = <input value="{{ userInput }}" />
// Renders: <input value='Hello'>
```

### Multi-line Inline HTML

An inline HTML string can span as many lines as you need. The newlines inside
the tags become part of the HTML output.

```wren
var card = <article>
  <h2>Title</h2>
  <p>{{ description }}</p>
</article>
```

### Pitfalls

**The opening tag cannot nest itself.** The outermost tag of an inline HTML
string must not appear as a direct child tag at any nesting level. Once the
tree starts with a *different* tag, the original tag can be used freely below
it. The parser checks this on the *whole* string, not just the first level,
and reports `Cannot nest <div> inside <div>` on the offending line.

```wren
// Wrong — <div> is the outermost tag AND a direct child
var bad = <div><div>Hello</div></div>
var alsoBad = <section><section>Nested</section></section>

// Correct — use a different tag as the outermost wrapper
var good = <div><section>Hello</section></div>

// Also correct — wrapping with a different tag breaks the conflict
var fine = <section>
  <div style="border:1px solid #ccc">
    <div style="background:#eee">
      Deeply nested same tags work once the outer tag differs.
    </div>
  </div>
</section>
```

**Mismatched closing tags are not validated.** The parser only matches the
closing tag against the *outermost* opening tag. Inner tags are never checked
for balance — the string ends at the first closing tag that matches the outer
tag, and everything before it is served verbatim, unclosed inner tags included:

```wren
// Compiles fine — <span> is never closed, the markup is served as-is
var bad = <div><span>Hello</div>

// Correct — close every tag yourself
var good = <div><span>Hello</span></div>
```

The browser receives `<div><span>Hello</div>` and auto-closes the `<span>`
when it renders, so the page usually looks fine. But the markup is not
validated, so write matching tags yourself. Anything after the first matching
close tag is outside the string and fails to compile:

```wren
// Compilation error — the string already ended at </div>
var alsoBad = <div><span>Hello</div></span>
```

**Invalid tag names.** Tag names must be lowercase, start with a letter, and
contain only letters, numbers, and hyphens. No underscores or uppercase:

```wren
// Wrong
var bad1 = <my_component>Invalid</my_component>
var bad2 = <MyElement>Invalid</MyElement>

// Correct
var good = <span class="badge">Valid</span>
var good2 = <my-element class="badge">Custom element</my-element>
```

**Interpolation depth** is limited to 9 nested levels.

**Forgetting `return`.** Without `return`, the response body is empty. The page
renders as blank.

**Files starting with `_` or `.` are private** — they return 403 if accessed
directly. Use them for imports and internal logic only.

---

## Conditional Rendering with `&&`

Wren's `&&` operator is the primary tool for conditional rendering. When the
left side is truthy, the right side is returned; otherwise the expression
evaluates empty.

### Class toggling

```wren
// filter is a query parameter string
var filter = Request.get("filter") || "all"

<a href="/" class="filter-tab {{ filter == "all" && "active" }}">All</a>
<a href="/?filter=active" class="filter-tab {{ filter == "active" && "active" }}">Active</a>
<a href="/?filter=completed" class="filter-tab {{ filter == "completed" && "active" }}">Completed</a>
```

When `filter == "all"` is true, `"active"` is inserted as the class value.
When false, nothing is inserted.

### Conditional HTML blocks

You can conditionally render entire chunks of HTML. The right side of `&&` can
be an inline HTML string. The `{{ }}` interpolation must always appear inside
an HTML tag (`{{ }}` outside an HTML tag is parsed as Wren map syntax).

```wren
return <main>
  {{ showClear && <form method="post" action="/clear" class="clear-form">
    <button class="clear-btn">Clear completed</button>
  </form> }}
</main>
```

```wren
return <main>
  {{ tasks.count == 0 && <section class="empty-state">
    <span class="empty-icon" aria-hidden="true">🎉</span>
    <p class="empty-text">Nothing here yet.</p>
  </section> }}
</main>
```

```wren
// Conditional text
<span class="stats-count">
  <strong>{{ activeCount }}</strong>
  task{{ activeCount != 1 && "s" }} remaining
</span>
```

### Pitfall

**Multiline expressions inside `{{ }}` are valid.** The Wren expression can
span lines, and HTML strings inside it can span lines too. Wren keeps one
newline rule: an infix operator ends its line, so put `&&`, `?`, `:`, and
similar at the end of a line. An operator at the start of a line is a parse
error (`Expected expression.`), exactly as in plain Wren.

```wren
// Valid — the expression spans lines, operators end each line
return <div>{{
  showClear &&
  <form method="post" action="/clear">
    <button>Clear</button>
  </form>
}}</div>

// Invalid — the operator starts a line
return <div>{{
  showClear
  && <form method="post" action="/clear">
    <button>Clear</button>
  </form>
}}</div>
```

Block callbacks (`map { |v| ... }`) follow Wren's single-expression rule: the
body must fit on the line after the `{`, otherwise the block returns `null`
and the output is empty.

---

## Ternary Expressions

Ternary operators are essential for switching attributes, classes, and entire
blocks of HTML based on a condition.

### Attribute switching

```wren
<button class="{{ task.finished ? "checkbox-btn checked" : "checkbox-btn" }}"
        title="{{ task.finished ? "Mark incomplete" : "Mark complete" }}"
        aria-label="{{ task.finished ? "Mark incomplete" : "Mark complete" }}">
</button>
```

### Nested ternaries for multi-value decisions

```wren
<span class="priority-dot {{ Num.fromString(task.id.toString) % 3 == 1 ? "high" : Num.fromString(task.id.toString) % 3 == 2 ? "medium" : "low" }}"
      aria-hidden="true"></span>
```

### Ternary with HTML operands

Each branch of a ternary can be an inline HTML string that spans multiple
lines. The `{{ }}` must be inside an HTML tag context:

```wren
return <main>{{ task.finished ? <span class="task-text completed">
  {{ task.description }}
</span> : <span class="task-text">
  {{ task.description }}
</span> }}</main>
```

### Pitfall

**Deeply nested ternaries hurt readability.** For complex logic, compute the
value above the template and use a simple variable instead:

```wren
// Compute before the template
var taskClass = "task-text"
if (task.finished) {
  taskClass = "task-text completed"
}

// Clean template
<span class="{{ taskClass }}">{{ task.description }}</span>
```

**Newlines outside HTML tags** — same rule as `&&`:

```wren
// Wrong — Wren code broken across lines outside the HTML string
return <div>{{
  task.finished ?
  <span class="completed">Done</span> :
  <span class="pending">Pending</span>
}}</div>

// Correct — one expression, newlines inside the HTML strings
return <div>{{ task.finished ? <span class="completed">
  Done
</span> : <span class="pending">
  Pending
</span> }}</div>
```

---

## Iteration with `map`

Wren's `map` function transforms each element of a list into an HTML string.
The callback body is a single Wren expression — but the inline HTML string
inside it can span many visual lines.

### Simple list

```wren
var items = ["Apple", "Banana", "Cherry"]
var html = <ul>{{ items.map { |item| <li>{{ item }}</li> } }}</ul>
// Output: <ul><li>Apple</li><li>Banana</li><li>Cherry</li></ul>
```

### Multiline items

The callback is one expression (`|task| <li>...</li>`), but the `<li>` inline
HTML string spans multiple lines. The Wren code stays on one line of Wren, but
the HTML inside the tags has newlines.

```wren
<ul class="task-list">
  {{ tasks.map{ |task| <li class="task-item {{ task.finished && "completed" }}">
    <section class="task-item-row">
      <form method="post" action="/toggle" class="checkbox-form">
        <input type="hidden" name="id" value="{{ task.id }}" />
        <button class="{{ task.finished ? "checkbox-btn checked" : "checkbox-btn" }}"
                title="{{ task.finished ? "Mark incomplete" : "Mark complete" }}"
                aria-label="{{ task.finished ? "Mark incomplete" : "Mark complete" }}">
        </button>
      </form>
      <span class="task-content">
        <span class="{{ task.finished ? "task-text completed" : "task-text" }}">
          <span class="priority-dot low" aria-hidden="true"></span>
          {{ task.description }}
        </span>
        <span class="task-meta">{{ task.createdAt.hh }}:{{ task.createdAt.mi }}</span>
      </span>
      <form method="post" action="/delete" class="delete-form">
        <input type="hidden" name="id" value="{{ task.id }}" />
        <button class="delete-btn" title="Delete task" aria-label="Delete task">✕</button>
      </form>
    </section>
  </li> } }}
</ul>
```

### Pitfalls

**The callback must be a single expression.** You cannot have multiple Wren
statements or variable declarations inside a `map` callback:

```wren
// Wrong — multiple statements inside map
{{ tasks.map { |task|
  var icon = task.finished ? "✓" : "○"
  <li>{{ icon }} {{ task.description }}</li>
} }}
```

If you need pre-computed values, prepare them before `map` (e.g. add computed
properties to your model class, or pre-process the list).

**Empty lists produce empty output.** Pair `map` with `&&` to show an empty
state:

```wren
return <main>
  <ul>
    {{ tasks.map{ |task| <li>{{ task.description }}</li> } }}
  </ul>
  {{ tasks.count == 0 && <section class="empty-state">
    <p>No tasks yet. Add your first one above.</p>
  </section> }}
</main>
```

**Large lists hurt performance.** Paginate or limit results in the controller —
`map` iterates everything you give it.

**Newlines outside HTML tags** — same rule:

```wren
// Wrong — Wren code broken across lines outside the HTML string
return <div>{{
  tasks.map { |task|
  <li>{{ task.description }}</li>
} }}</div>

// Correct — the expression starts on the {{ line
return <div>{{ tasks.map{ |task| <li>
  {{ task.description }}
</li> } }}</div>
```

---

## Bialet MVC: File Structure

Now that you know how to write HTML, here's how a complete `.wren` file is
organized. Bialet follows an MVC-like pattern within a single file:

- **Model** — Wren classes in separate files (e.g. `_domain.wren`), imported at
  the top
- **Controller** — the logic at the top of the `.wren` file: routing decisions,
  POST handling, data fetching, redirects
- **View** — the inline HTML at the bottom, passed to `return` to send the
  response and end the script

```wren
// === Controller ===
import "_template" for Template
import "_domain" for Post

if (Request.isPost) {
  var post = Post.new()
  post.title = Request.post("title") || ""
  post.save()
  return Response.redirect("/")
}

var posts = Post.list()

// === View ===
return Template.new().layout(<main>
  <h1>Posts</h1>
  <ul>
    {{ posts.map{ |p| <li><a href="/posts/{{ p.id }}">{{ p.title }}</a></li> } }}
  </ul>
</main>)
```

### Pitfall

**`return` terminates immediately.** Code after `return` never executes.
This means you cannot `return` and then run more logic. Put all controller
logic before the view.

---

## Model: Wren Classes

Wren classes serve as the Model layer. They encapsulate data, database queries,
and domain logic — keeping it out of your template code.

```wren
// _domain.wren
class Post {
  construct new(row) {
    _id = row["id"]
    _title = row["title"]
    _body = row["body"]
    _createdAt = row["createdAt"] || Date.now
  }

  static new() { Post.new({}) }

  id { _id }
  title { _title }
  title=(val) { _title = val.toString.trim() }
  body { _body }
  body=(val) { _body = val.toString() }
  createdAt { Date.new(_createdAt) }

  save() { `Post`.save(this) }

  static list() { `
    SELECT * FROM Post ORDER BY createdAt DESC
  `.fetch.to(Post) }

  static find(id) { `
    SELECT * FROM Post WHERE id = ?
  `.first(id).to(Post) }
}
```

Now the controller imports this class and the view iterates over `Post`
instances — clean, testable, and separated from presentation.

### Pitfall

**Database values come back as strings.** Always convert before numeric
operations:

```wren
var count = Num.fromString(row["cnt"])
// Wrong: row["cnt"] + 1  —  this concatenates strings
```

---

## Controller: Logic at the Top

The top of each `.wren` file handles the request. This is where you:

1. Check the HTTP method (`Request.isPost`)
2. Read form data (`Request.post("field")` — returns `null` when missing, use `|| ""`)
3. Read query parameters (`Request.get("key")` — also returns `null`, handle accordingly)
4. Call model methods
5. Redirect or prepare data for the view

```wren
import "_template" for Template
import "_domain" for Post

if (Request.isPost) {
  var post = Post.new()
  post.title = Request.post("title") || ""
  post.body = Request.post("body") || ""
  post.save()
  return Response.redirect("/posts/" + post.id.toString)
}

var postId = Request.route(1)
var post = Post.find(postId)
```

Use `return Response.redirect(path)` to redirect after POST — this follows
the Post/Redirect/Get pattern and prevents duplicate form submissions.

### Pitfall

**Forgetting `return` before `Response.redirect()`** causes a double-response
error. The redirect sends headers, then the code below still executes and
tries to send a body. Always `return Response.redirect(...)`.

```wren
// Wrong — missing return
if (Request.isPost) {
  task.save()
  Response.redirect("/")  // redirect sends headers...
}
// ...then this still runs, trying to send a second response
return Template.new().layout(...)
```

---

## Template Classes / Components

Extract shared HTML into Wren classes. Each method returns an inline HTML
string, turning your class into a library of reusable components.

### Layout template

Create `_template.wren` and import it from your pages:

```wren
class Template {
  construct new() {
    _title = "My App"
    _subtitle = "A Bialet application"
  }

  layout(content) { <!doctype html>
    <html lang="en">
      {{ head(_title) }}
      <body>
        <header class="app-header">
          <h1>{{ _title }}</h1>
          <p class="app-subtitle">{{ _subtitle }}</p>
        </header>
        <main>{{ content }}</main>
        {{ footer }}
      </body>
    </html> }

  head(title) { <head>
    <title>{{ title }}</title>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <link rel="stylesheet" href="/style.css" />
  </head> }

  footer { <footer class="app-footer">
    <p>Built with <a href="https://bialet.dev">Bialet</a>.</p>
  </footer> }
}
```

### Using the template

```wren
// index.wren
import "_template" for Template

return Template.new().layout(<main>
  <h1>Welcome</h1>
  <p>This content goes into {{ content }}.</p>
</main>)
```

A Wren block that contains a single expression implicitly returns that
expression — no `return` keyword needed in the method body.

### Pitfalls

**Instance methods need `new()`.** If your methods are not `static`, you
must instantiate the class:

```wren
// Wrong — calls static layout, doesn't exist
return Template.layout(...)

// Correct — creates an instance first
return Template.new().layout(...)
```

**Don't over-engineer.** A template class with a handful of methods (`layout`,
`head`, `footer`, plus one or two component helpers) covers most needs.
Nested class hierarchies and deep abstraction layers work against Bialet's
simplicity.

---

## Semantic HTML

Prefer semantic elements that describe their purpose. Bialet's inline HTML is
plain HTML — the browser treats it exactly as if you'd written an `.html` file.
Use the right element for the job.

| Instead of `<div>` | Use |
|---|---|
| Page header / branding | `<header>` |
| Navigation links | `<nav>` |
| Main content area | `<main>` |
| Self-contained content block | `<article>` |
| Thematic grouping | `<section>` |
| Sidebar or complementary content | `<aside>` |
| Page footer | `<footer>` |

A real-world page using semantic elements:

```wren
return Template.new().layout(<main>
  <form method="post">
    <section class="input-group">
      <input name="task" placeholder="What needs to be done?" required autofocus />
      <button type="submit">Add</button>
    </section>
  </form>

  <nav class="filters" aria-label="Task filters">
    <a href="/" class="filter-tab active">All</a>
    <a href="/?filter=active" class="filter-tab">Active</a>
    <a href="/?filter=completed" class="filter-tab">Completed</a>
  </nav>

  <aside class="stats-bar">
    <span><strong>3</strong> tasks remaining</span>
  </aside>

  <article class="task-card">
    <h2>Buy groceries</h2>
    <p>Added 10 minutes ago</p>
  </article>

  <footer class="app-footer">
    <p>Built with Bialet</p>
  </footer>
</main>)
```

Semantic HTML improves accessibility (screen readers understand page structure),
SEO (crawlers parse meaningful sections), and readability (future you will
thank you).

### Pitfall

**Same-tag nesting rule still applies.** The outermost tag of the inline
HTML string cannot repeat at any nesting level:

```wren
// Wrong — <section> is outermost and repeats deeper
var bad = <section><section>Nested section</section></section>

// Correct — wrap with a different semantic tag
var good = <main>
  <article>
    <section>One block</section>
    <section>Another block</section>
  </article>
</main>
```

---

## Styling & CSS

Bialet outputs plain HTML. CSS works exactly as it does with static HTML files.
If you're coming from React and haven't written vanilla CSS in a while, here's
what you need to know.

### Adding a stylesheet

Place your `.css` file in the app root (next to your `.wren` files) and link it
in your template's `<head>`:

```wren
head { <head>
  <title>{{ _title }}</title>
  <meta charset="utf-8" />
  <link rel="stylesheet" href="/style.css" />
</head> }
```

### Writing CSS

Use CSS custom properties for theming and write class-based selectors targeting
your semantic HTML elements. Here's a minimal starting point:

```css
/* style.css */
:root {
  --bg: #f8fafc;
  --card-bg: #ffffff;
  --text: #1e293b;
  --accent: #14b8a6;
  --radius: 14px;
}

body {
  font-family: system-ui, sans-serif;
  background: var(--bg);
  color: var(--text);
  padding: 2rem 1rem;
}

main {
  max-width: 540px;
  margin: 0 auto;
}

header {
  margin-bottom: 2rem;
}

.card {
  background: var(--card-bg);
  border-radius: var(--radius);
  padding: 1rem;
  box-shadow: 0 1px 3px rgba(0,0,0,0.06);
}
```

### Semantic CSS first (Bialet philosophy)

Target elements directly by tag name where possible, then add classes for
variants. This keeps both your HTML and CSS lean:

```css
/* Base: all nav elements get a consistent look */
nav {
  display: flex;
  gap: 0.25rem;
  background: var(--card-bg);
  border-radius: var(--radius);
  padding: 0.25rem;
}

/* Variant: only the filter nav gets extra styling */
nav.filters {
  margin-bottom: 1.25rem;
}
```

### PicoCSS

PicoCSS is a classless CSS framework that styles semantic HTML elements
automatically. It pairs perfectly with Bialet's semantic HTML approach — you
write `<nav>`, `<main>`, `<article>`, and PicoCSS handles the styling with zero
classes.

```wren
head { <head>
  <title>{{ _title }}</title>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/@picocss/pico@2/css/pico.min.css" />
</head> }
```

With PicoCSS loaded, your semantic HTML gets a clean, consistent look without
writing any CSS. Add a custom stylesheet alongside it for app-specific
overrides.

### Tailwind CSS

Tailwind works in Bialet — it's just classes on HTML elements. For prototyping,
use the CDN script:

```wren
head { <head>
  <title>{{ _title }}</title>
  <meta charset="utf-8" />
  <script src="https://cdn.tailwindcss.com"></script>
</head> }
```

Then use utility classes directly:

```wren
<main class="max-w-lg mx-auto py-8 px-4">
  <h1 class="text-2xl font-bold mb-6">Dashboard</h1>
  <article class="bg-white rounded-xl shadow-sm p-4">
    <p class="text-gray-600">Hello World</p>
  </article>
</main>
```

The CDN is fine for tinkering, but it renders styles in the browser at runtime
and adds an external dependency. For production, self-host the compiled CSS
with the Tailwind CLI.

#### Self-hosting with the Tailwind CLI

This works because Tailwind scans your files as plain text for class names. The
classes in your `.wren` files are standard HTML attributes, so no special
configuration is needed to detect them.

**1. Install the CLI.** In your app directory:

```bash
npm install -D tailwindcss @tailwindcss/cli
```

**2. Create an input stylesheet.** This is the source file Tailwind reads.
Put it in a `_`-prefixed directory so Bialet never serves it (see the pitfalls
below) but Tailwind can still read it from disk. Create `_src/input.css`:

```css
@import "tailwindcss";
@source "./**/*.wren";
```

The `@source` directive tells Tailwind to scan your `.wren` files for classes.
Adjust the glob to match your app's layout if you keep pages in subdirectories.

**3. Link the compiled output.** Reference the built file in your template's
`head`. Bialet serves it as a normal static file:

```wren
head { <head>
  <title>{{ _title }}</title>
  <meta charset="utf-8" />
  <link rel="stylesheet" href="/style.css" />
</head> }
```

#### Development: watch mode

Run Tailwind and Bialet side by side, in two terminals:

```bash
# Terminal 1 — start Bialet
bialet .

# Terminal 2 — compile CSS on every change
npx @tailwindcss/cli -i _src/input.css -o style.css --watch
```

With [live reload](live-reload.md) enabled, Bialet watches the whole app
directory. Every time Tailwind writes a new `style.css`, the browser reloads
automatically — a full Tailwind + Bialet dev loop with no extra tooling.

#### Production: one-shot build

Compile once, minified:

```bash
npx @tailwindcss/cli -i _src/input.css -o style.css --minify
```

Commit `style.css` and deploy it alongside your `.wren` files. The server needs
no Node.js — the compiled file is just a static asset. For caching and gzip on
the compiled CSS, see [Deployment](deployment.md).

### Pitfalls

**No CSS preprocessors built in.** Bialet doesn't bundle Sass, Less, or
PostCSS. Write vanilla CSS, or use a separate build step (like Tailwind's CLI)
for preprocessing.

**`_`-prefixed CSS files are not served.** Files starting with `_` or `.`
return 403. Name your stylesheets without a leading underscore:

```wren
// ✓ Served
<link rel="stylesheet" href="/style.css" />
<link rel="stylesheet" href="/theme.css" />

// ✗ Returns 403
<link rel="stylesheet" href="/_style.css" />
```

This is why the input file lives in `_src/` — Bialet blocks HTTP access to it
while the CLI still reads it from disk. The output file (`style.css`) must not
start with `_` or `.`.

**`node_modules` is publicly served.** Everything inside the app root is served
unless a path component starts with `_` or `.`. After `npm install`, the
`node_modules/` directory is reachable over HTTP. Block it at your reverse
proxy in production (see [Deployment](deployment.md)), or run the npm project
outside the app root and point the CLI at it.

**Dynamic class names are not compiled.** Tailwind scans your source as plain
text, so it cannot see classes assembled at runtime. This compiles fine:

```wren
class="text-red-600"   // ✓
```

This does not — Tailwind never sees the full class name:

```wren
class="text-{{ error ? "red" : "green" }}-600"   // ✗
```

Use complete class names in your `.wren` files.

**Framework CDNs add an external dependency.** For production, compile and serve
the CSS from your app directory instead of loading it from a CDN.

---

## Escaping & Security

The `{{ }}` interpolation **escapes HTML by default**. Plain values — strings
from user input, database queries, or URL parameters — have `&`, `<`, `>`, `"`,
and `'` replaced with their HTML entities before they reach the page.

```wren
var userInput = "<script>alert('xss')</script>"

// Safe — HTML characters are escaped automatically
var safe = <p>{{ userInput }}</p>
// Renders: <p>&lt;script&gt;alert(&#x27;xss&#x27;)&lt;/script&gt;</p>
```

You can also use `Util.htmlEscape()` for manual escaping outside of
interpolation:

```wren
var escaped = Util.htmlEscape(userInput)
```

Markup that is already safe is left untouched: HTML string literals, the
result of a nested `{{ }}` block, `HtmlNode` values, and `String.raw`. See
[Security](security.md) for the full rules.

> ⚠️ Pitfall: **Do not add `.safe` inside `{{ }}`.** Interpolation already
> escapes, so `{{ userInput.safe }}` escapes the text twice (`&amp;lt;`).
> Use `HtmlNode` or `.raw` only for markup you intentionally trust.

---

## Common Pitfalls

- **Opening-tag nesting:** The outermost tag of an inline HTML string cannot
  appear again as a child at any level. This means `<div><div>...</div></div>` fails because `<div>` is both the outermost and a nested tag. Wrap with a different tag (e.g. `<section>`) and nest freely below it:
  `<section><div><div>...</div></div></section>`.
- **Mismatched tags are not validated:** Only the outer closing tag is
  matched; `<div><span>Hello</div>` compiles and is served verbatim (the
  browser auto-closes the `<span>`). Write well-formed HTML yourself.
- **Invalid tag names:** Tag names must be lowercase, start with a letter, and
  contain only letters, numbers, and hyphens — no underscores or uppercase.
  Hyphens enable custom elements like `<my-element>`. Use classes and semantic
  tags otherwise.
- **Interpolation depth:** Maximum 9 nested `{{ }}` levels.
- **Forgetting `return`:** Without `return`, the response body is empty.
- **`_`/`.`-prefixed files:** Private, return 403 if accessed directly.
- **Newlines inside `{{ }}`:** The Wren expression can span lines, but an
  infix operator must end its line — a leading operator is a parse error.
- **Map callback is a single expression:** The body must fit on the line
  after the `{`, otherwise the block returns `null` and renders empty.
- **Empty lists in `map`:** Produce empty output — pair with `&&` for empty
  states.
- **`return` terminates immediately:** Code after `return` never executes.
- **DB values are strings:** Convert with `Num.fromString()` before math.
- **Forgetting `return` before `Response.redirect()`:** Causes double-response
  errors.
- **Instance vs static methods:** Instance methods need `new()`; static methods
  don't.
- **`_`-prefixed CSS is not served:** Name stylesheets without leading `_`.
- **CSS preprocessors not built in:** Write vanilla CSS or use a build step.
- **Framework CDNs in production:** Self-host CSS files for reliability.
- **`{{ }}` escapes by default:** Plain values are auto-escaped; do not add
  `.safe` inside `{{ }}` (it double-escapes). Use `HtmlNode` or `.raw` for
  intentionally-raw markup.
