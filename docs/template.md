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

HTML's void elements (`<br>`, `<hr>`, `<img>`, `<input>`, `<meta>`, ...) are
accepted with or without a trailing slash, and the markup is served verbatim:

```wren
var a = <div>one<br/>two</div>   // <br/> kept as written
var b = <div>one<br />two</div>  // <br /> kept as written
var c = <div>one<br>two</div>    // <br> kept as written
```

When a void element is the *outermost* tag of the string it must end with a
space and slash — `<input value="{{ val }}" />` — so the parser knows the
string has ended. The output omits the space and slash:

```wren
var inputField = <input value="{{ userInput }}" />
// Renders: <input value='Hello'>
```

Without the space (`<input/>`) the parser reads the slash as part of the tag
name and rejects it as `Invalid tag name: must be lowercase alphanumeric +
hyphens` instead of ending the string.

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

**The outermost tag may nest itself.** `<div><div>Hello</div></div>` is a
valid string: the lexer tracks how many copies of the outermost tag name are
open and ends the string only at the close tag that matches the outermost
opening tag. Whether the string starts with `<div>` or `<section>` makes no
difference, and any tag can appear inside any other one.

```wren
var two = <div><div>Hello</div></div>
var three = <div><div><div>Deep</div>Middle</div>Outer</div>
var mixed = <section><div><section>Also fine</section></div></section>

// Siblings and interpolation work the same way
var siblings = <div><div>One</div><div>Two</div></div>
var list = <ul>{{ items.map{|item| <li>{{ item }}</li> } }}</ul>
```

Each level of the *outermost* tag must be closed. A missing close tag or one
close tag too many is a compile error, and the message says which one you hit.
A close tag with a different name is not validated at all: it only fails when it
leaves the outermost tag one close short.

```wren
// Compilation error
// Unclosed <div>: 2 open, 1 closed (string opened on line 1).
var bad = <div><div>Hello</div>

// Compilation error — the outer <div> is one close short; the message names
// the </section> that was mistaken for it
// Unclosed <div>: 2 open, 1 closed. </section> does not close <div>
// (string opened on line 1).
var alsoBad = <div><div>Hello</section></div>

// Compiles: </section> is not validated, and the outer <div> is closed twice.
// The served markup is unbalanced — the browser repairs it.
var accepted = <div><div>Hello</section></div></div>

// Compilation error
// Unexpected </div>: <div> is already closed.
var stillBad = <div><div>Hello</div></div></div>
```

The `Unclosed` message counts how many copies of the outermost tag were opened
and closed, then names the line the literal started on — the error itself is
reported where the file ended. When the literal was split by `{{ }}`, the line
is still the one the literal opened on, not the line it resumed on.

An opening tag cut short by a newline or the end of the file is a different
error, because there is no content to read. It covers both a cut inside the tag
name and one after it, in the attributes:

```wren
// Compilation error — Expected '>' to close the opening <div> tag.
var cut = <div

// Compilation error — same message: the name is complete, the tag is not.
var cutAttributes = <div class="a"
```

**Mismatched closing tags are not validated.** The parser only tracks the tag
name it is matching against. Inner tags with a different name are never checked
for balance — everything before the outermost close tag is served verbatim,
unclosed inner tags included:

```wren
// Compiles fine — <span> is never closed, the markup is served as-is
var bad = <div><span>Hello</div>

// Correct — close every tag yourself
var good = <div><span>Hello</span></div>
```

The browser receives `<div><span>Hello</div>` and auto-closes the `<span>`
when it renders, so the page usually looks fine. But the markup is not
validated, so write matching tags yourself. Anything after the first matching
close tag is outside the string and fails to compile. A second close tag for
the outermost name gets a clear message; one for any other name is only seen as
stray tokens:

```wren
// Compilation error — Unexpected </div>: <div> is already closed.
var alsoBad = <div><span>Hello</div></div>

// Compilation error — Error at '/': Expected expression.
var alsoBad2 = <div><span>Hello</div></span>
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

**`return <tag>` is valid at the top level and inside a multi-line method body.**
`return` and the opening `<tag>` must share a physical line:

```wren
// Top level
return <p>Hola</p>

// Inside a method body
greeting() {
  return <p>Hola</p>
}
```

Wren ends an expression at a newline, Python-style. A bare `return` followed by
a newline returns immediately, and the tag on the next line is a separate
expression that never reaches the response:

```wren
// Wrong — returns null, then evaluates the tag as a statement
return
  <p>Hola</p>
```

At the top level the code above compiles and returns an empty body. Inside a
block or a method body it can also fail with a confusing `Expect newline after
statement` error. Either way, keep `return` and the tag on the same line.

**`return` is not allowed inside a block callback.** A block body is a single
expression, and `return` is a statement, so `list.map { |x| return <div>...</div> }`
fails to compile with `Error at 'return': Expected expression.` Use the
expression itself, without `return`:

```wren
// Wrong — return is a statement, not an expression
list.map { |x| return <div>{{ x }}</div> }

// Correct — the block returns its expression implicitly
list.map { |x| <div>{{ x }}</div> }
```

A method body written entirely on one line (`{ return 1 }`) is a
single-expression body, so `return` fails there too. Put the statement on its
own line inside a multi-line body:

```wren
// Wrong — the braces are a single-expression body
greeting() { return "Hola" }

// Correct — a multi-line statement body
greeting() {
  return "Hola"
}
```

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

A block body is one expression, so it returns its value. When the body is an
HTML literal it may start on the line after `{`: HTML is never a statement, so
the multi-line form is unambiguous and renders.

```wren
{{ tasks.map { |task|
  <li>{{ task.description }}</li>
} }}
```

A body that is *code* must still start on the `{` line. A body that starts on
the next line is a statement list whose value is discarded, so the block
returns `null`; inside `{{ }}` that renders nothing, and the compiler warns:

```text
Warning: app/index.wren:12: block body starts on the line after '{', so its
value is discarded and this interpolation renders nothing; keep the body on the
same line as '{', or make the body an HTML literal
```

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
<span class="priority-dot {{ task.id.toNum % 3 == 1 ? "high" : task.id.toNum % 3 == 2 ? "medium" : "low" }}"
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
The callback body is a single Wren expression — either on the `{` line, or an
HTML literal starting on the next line. The literal itself can span many visual
lines.

### Simple list

```wren
var items = ["Apple", "Banana", "Cherry"]
var html = <ul>{{ items.map { |item| <li>{{ item }}</li> } }}</ul>
// Output: <ul><li>Apple</li><li>Banana</li><li>Cherry</li></ul>
```

### Multiline items

The callback is one expression (`|task| <li>...</li>`), but the `<li>` inline
HTML string spans multiple lines. The Wren expression may stay on one line, and
the HTML literal may equally begin on the line after `|task|`; the HTML inside
the tags has newlines either way.

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

**The callback must yield a single expression.** A `var` turns the body into a
statement list, and a statement that ends in an HTML literal cannot close the
block, so this form does not compile (`Expect newline after statement`):

```wren
// Wrong — does not compile
{{ tasks.map { |task|
  var icon = task.finished ? "✓" : "○"
  <li>{{ icon }} {{ task.description }}</li>
} }}
```

A Wren block body is **one expression**. An HTML body may start on the line
after `{`, but a code body must start on the same line: `{ |x|` followed by a
newline begins a statement list, which returns `null`. In a template the
compiler warns when that happens, because the interpolation then renders
nothing. Keep a code body on the same line as `{`, and an HTML body may be
written either way:

```wren
{{ tasks.map { |task| <li>{{ task.description }}</li> } }}

{{ tasks.map { |task|
  <li>{{ task.description }}</li>
} }}
```

`var` is a statement, so it cannot appear in the block body even on one line.
Pre-compute the value before `map` (a computed property on the model, or a
pre-processed list).

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

**Newlines outside HTML tags** — an expression can start on the next line
inside `{{ }}`, but `return` still ends at its newline:

```wren
// Valid — the interpolation expression starts on the next line
return <div>{{
  tasks.map { |task| <li>{{ task.description }}</li> }
}}</div>

// Wrong — `return` on its own line returns immediately
return
  <div>Nothing here is returned</div>
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

**Forgetting `return` before `Response.redirect()`.** Code after a bare
`Response.redirect(...)` still runs. There is no error — the server sends the
302 with its `Location` header and attaches whatever body the rest of the
script returns. Always `return Response.redirect(...)` so the script stops at
the redirect and no body is sent:

```wren
// Wrong — the code below still runs and attaches a body to the 302
if (Request.isPost) {
  task.save()
  Response.redirect("/")
}
return Template.new().layout(...)

// Correct — return stops the script at the redirect
if (Request.isPost) {
  task.save()
  return Response.redirect("/")
}
return Template.new().layout(...)
```

---

## Template Classes / Components

A component is a Wren class with a method that returns an inline HTML block.
You compose markup by calling one method from another inside `{{ }}`. That is
the entire mechanism: no registry, no props object, no separate template file.

```wren
class Badge {
  static rule(text) { <span class="badge">{{ text }}</span> }
}

return <p>Status: {{ Badge.rule("ready") }}</p>
```

Extract shared HTML into Wren classes. Each method returns an inline HTML
string, turning your class into a library of reusable components. A layout is
the first component of every page, not a special mechanism: `layout(content)`
returns a page and takes the body as a parameter. Every smaller piece follows
the same shape.

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
    <p>&copy; My App</p>
  </footer> }
}
```

### Using the template

```wren
// index.wren
import "_template" for Template

return Template.new().layout(<main>
  <h1>Welcome</h1>
  <p>This body is passed to layout() and inserted into the page.</p>
</main>)
```

A Wren block that contains a single expression implicitly returns that
expression — no `return` keyword needed in the method body.

### A component set

Split the page by region: one class per region, one file per class. The
`_app/components/` folder is a convention, not a framework requirement —
anything behind a `_` path segment is importable and is never served.

```text
my-app/
├── _app/
│   ├── components/
│   │   ├── cards.wren      # Cards.tool, Cards.mission
│   │   ├── hud.wren        # Hud.state
│   │   ├── nav.wren        # Nav.bar
│   │   └── layout.wren     # Layout.page
│   ├── domain.wren
│   └── migration.wren
├── dashboard.wren          # /
└── missions.wren           # /missions
```

```wren
// _app/components/cards.wren
class Cards {
  // One tool in the grid: name, wear, and a warning when it is broken.
  static tool(item) { <article class="card tool">
    <h3>{{ item["name"] }}</h3>
    <p class="meta">Wear {{ item["wear"] }} of 100</p>
    {{ item["broken"] == "1" && <p class="warn">Broken. Repair it before the next run.</p> }}
  </article> }

  // One mission row: link and reward.
  static mission(row) { <li class="mission">
    <a href="/mission?id={{ row["id"] }}">{{ row["title"] }}</a>
    <span class="reward">{{ row["reward"] }} gold</span>
  </li> }
}
```

```wren
// _app/components/hud.wren
class Hud {
  static state(game) { <aside class="hud">
    <dl class="hud-facts">
      <dt>Day</dt><dd>{{ game["day"] }}</dd>
      <dt>Fuel</dt><dd>{{ game["fuel"] }} of 100</dd>
      <dt>Credits</dt><dd>{{ game["credits"] }}</dd>
    </dl>
    {{ Num.fromString(game["fuel"]) < 20 && <p class="warn">Fuel is low.</p> }}
  </aside> }
}
```

```wren
// _app/components/nav.wren
class Nav {
  static bar(user) { <nav class="bar" aria-label="Main">
    <a class="brand" href="/">Salvage</a>
    <a href="/">Dashboard</a>
    <a href="/missions">Missions</a>
    {{ user && <span class="who">{{ user["name"] }}</span> }}
  </nav> }
}
```

```wren
// _app/components/layout.wren
import "/_app/components/nav" for Nav

class Layout {
  static page(title, user, content) { <!doctype html>
    <html lang="en">
      <head>
        <meta charset="utf-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1" />
        <title>{{ title }} · Salvage</title>
        <link rel="stylesheet" href="/style.css" />
      </head>
      <body>
        {{ Nav.bar(user) }}
        <main>
          {{ content }}
        </main>
      </body>
    </html> }
}
```

`Layout.page` is a component like any other. It calls `Nav.bar` from inside its
own markup, and that is how nesting works: a method call inside `{{ }}`.

### The route that renders them

The route reads input, fetches the rows, and passes them in. It holds no markup
beyond the sections the page itself is made of.

```wren
// dashboard.wren — GET /
import "_app/components/layout" for Layout
import "_app/components/hud" for Hud
import "_app/components/cards" for Cards

var userId = Session.new().get("user_id")
var user = `SELECT id, name FROM users WHERE id = ?`.first(userId)
if (!user) return Response.redirect("/login")

var game = `SELECT day, fuel, credits FROM games WHERE user_id = ?`.first(user["id"])
if (!game) return Response.notFound()

var tools = `SELECT id, name, wear, broken FROM tools WHERE user_id = ?`.fetch(user["id"])
var missions = `SELECT id, title, reward FROM missions ORDER BY reward DESC LIMIT 3`.fetch

return Layout.page("Dashboard", user,
  <section class="dashboard">
    {{ Hud.state(game) }}
    <h2>Tools</h2>
    <div class="grid">{{ tools.map { |t| Cards.tool(t) } }}</div>
    <h2>Next missions</h2>
    <ul class="missions">{{ missions.map { |m| Cards.mission(m) } }}</ul>
  </section>
)
```

### The same markup in a second page

`missions.wren` renders the same mission rows. It calls the same method instead
of repeating the `<li>`:

```wren
// missions.wren — GET /missions
import "_app/components/layout" for Layout
import "_app/components/cards" for Cards

var userId = Session.new().get("user_id")
var user = `SELECT id, name FROM users WHERE id = ?`.first(userId)
if (!user) return Response.redirect("/login")

var missions = `SELECT id, title, reward FROM missions ORDER BY reward DESC`.fetch

return Layout.page("Missions", user,
  <ul class="missions">{{ missions.map { |m| Cards.mission(m) } }}</ul>
)
```

A third page that needs the row is a third call. The `<li>` exists once.

### Pass data in, do not fetch it inside

Components take what they render as parameters — a row, a Map, a string, a
number. The route decides which row that is.

```wren
<main>{{ Cards.tool(item) }}</main>   // the row arrives as a parameter
```

```wren
// Wrong — the component owns a session lookup, a query and an id
class Cards {
  static tool(id) {
    var userId = Session.new().get("user_id")
    var item = `SELECT * FROM tools WHERE id = ? AND user_id = ?`.first(id, userId)
    return <article class="card tool">
      <h3>{{ item["name"] }}</h3>
    </article>
  }
}
```

The second version cannot be reused on a page that already has the row, runs a
query in the middle of rendering, and fails when there is no request behind it —
in a unit test or a cron job, for example. See [Testing](tests.md) for the
white-box pattern.

### Keep components pure

A component renders its parameters and may call other components. It does not:

- read `Request`, `Session`, `Cookie` or `Config`
- run SQL or write to the database
- set `Response.status`, redirect, or return a response object

The route does the rest: read input, fetch, decide (404, redirect, empty
state), then render. A pure component can be called with a plain Map in a test
and reused by any route that has the same data.

### Static or instance methods

Use `static` when the component carries no state: `Cards.tool(item)`,
`Hud.state(game)`, `Nav.bar(user)`.

Use instance methods when the class is configured once and read by several
methods — the `Template` example above stores `_title` and `_subtitle` in the
constructor. Then instantiate it in the route and call the methods on the
instance.

Both styles are fine. Do not mix them in one class for no reason.

### Looping over rows

`map` turns a list of rows into a list of components. Interpolation flattens
the list into markup:

```wren
<div class="grid">{{ tools.map { |t| Cards.tool(t) } }}</div>
```

The block body is a single expression. `Cards.tool(t)` is code, not an HTML
literal, so it must stay on the `{` line. A body that starts on the next line is
a statement list: it returns `null`, the interpolation renders nothing, and the
compiler warns.

```wren
// Wrong — the call starts on the next line
<div class="grid">{{ tools.map { |t|
  Cards.tool(t)
} }}</div>

// Correct
<div class="grid">{{ tools.map { |t| Cards.tool(t) } }}</div>
```

The same rule keeps `var` out of the block: format the value in the domain
class or before the template.

An empty list renders nothing. Pair it with an empty state:

```wren
<main>
  {{ tools.isEmpty && <p class="empty">No tools yet. Craft one first.</p> }}
</main>
```

### Escaping

Interpolation escapes plain values. A component returns `HtmlNode` — the type
of every inline HTML literal — so interpolation inserts it as markup:

```wren
<main>{{ Cards.tool(item) }}</main>   // the component's markup, rendered
<p>{{ item["name"] }}</p>             // a value, escaped
```

Inside the component, every database value still goes through `{{ }}` and is
escaped once. The only raw part is the tags you wrote yourself.

> ⚠️ Pitfall: never assemble markup as a string and mark it safe. The
> concatenation loses the compile-time tag checks, and `.raw` turns the user
> data embedded in it into markup:

```wren
// Wrong — XSS in one line
static tool(item) { ("<article>" + item["name"] + "</article>").raw }
```

`HtmlNode` and `.raw` assert that a string is already safe; they do not sanitize
it. See [Security](security.md) for the full rules.

### Where component files live

A component file lives behind a `_` path segment: it is importable, and
requesting it over HTTP returns 403. `_app/components/cards.wren` and
`_cards.wren` both qualify; that is all a component file needs.

- `_app/components/cards.wren` — one class per file, one file per region of the
  page. The folder name is yours; this is a convention.
- `_components/cards.wren` or `_cards.wren` at the app root — the same thing,
  flatter.
- `_template.wren` or `_app/template.wren` — one file with the layout and a few
  helpers, for a small app.

Import paths are relative to the importing file:

```wren
// dashboard.wren (app root)
import "_app/components/cards" for Cards

// admin/index.wren (one level down)
import "/_app/components/cards" for Cards

// _app/components/layout.wren (inside the folder)
import "/_app/components/nav" for Nav
```

The leading `/` form resolves from the app root from any directory. Use it
inside `_app/` and in other subfolders. Without the slash, the path starts at
the importing file's directory and works only from the root.

### Structuring a larger app

```text
my-app/
├── _app/                    # never served: template, domain, data
│   ├── components/          # UI: one class per concern
│   │   ├── layout.wren
│   │   ├── nav.wren
│   │   ├── hud.wren
│   │   └── cards.wren
│   ├── domain.wren          # data and rules
│   ├── migration.wren
│   └── cron.wren
├── dashboard.wren           # routes: read input -> call domain -> render
└── missions.wren
```

- **`_app/` holds everything that is not served.** Template and components,
  domain classes, migration, cron. Route files stay at the root, or in the
  folder that matches their URL.
- **One class per UI concern.** `Cards`, `Hud`, `Nav`, `Layout`. When a template
  class grows a second region's markup, split it by concern — do not keep adding
  methods to one god `Template`.
- **A route file is a controller.** Read input, call the domain, render a
  component. If a route contains a `<li>` that another route also needs, the
  `<li>` is in the wrong place.
- **A rule belongs to the domain, a tag belongs to the component.**
  `game.fuelLow`, `Mission.upcoming(limit)` and `Tool.wearLabel` are domain
  members; `Hud.state` and `Cards.tool` only render them.
- **A presentation helper used by two components is a component method.** A rule
  used by two routes is a domain method.

| The helper... | Goes in |
|---|---|
| formats or decides a value (`fuelLow`, `rewardLabel`) | the domain class |
| emits tags (a badge, an avatar, a formatted row) | a component method |
| fetches rows for a page | the route, or a `static` domain method |
| checks a permission | a domain or `Auth`-style class, called by the route |

### Does Bialet have components?

Yes. A component is a class whose method returns an inline HTML block.

There is no component registry, no props system, no slots, no JSX, no lifecycle
and no client-side runtime. That is intentional: Wren already has classes and
methods, and an inline HTML literal already returns markup. Composition is a
method call inside `{{ }}`; props are method parameters; the body is the
`content` parameter `Layout.page` already takes.

Nothing in the framework extracts repeated markup for you. If two pages repeat
the same card, status bar or navigation, that is app debt: move the markup into
a method and call it from both.

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

Extract on the second or third repetition, and only when the markup carries a
decision — a loop, a condition, a class toggled by data. A method per `<span>`
is worse than the markup it replaces.

**String-concatenated markup.** HTML is a literal, not a string to assemble:

```wren
// Wrong — no tag checks, and the value is now markup
static mission(row) { ("<li>" + row["title"] + "</li>").raw }

// Correct — the literal is checked at compile time, the value is escaped
static mission(row) { <li>{{ row["title"] }}</li> }
```

**An HTML argument starts on a new line.** The lexer opens an inline HTML
string after `=`, `return`, `(`, `[`, `{`, `|`, `:`, `&&`, `?` or a newline —
not after a comma. A literal written straight after a comma is read as a
less-than comparison:

```wren
// Wrong — "<section>" is parsed as a comparison
return Layout.page("Dashboard", user, <section class="dashboard"></section>)

// Correct — the newline opens the literal
return Layout.page("Dashboard", user,
  <section class="dashboard"></section>)
```

**Components that silently depend on globals.** A component that reads
`Session` or `Request` looks pure and is not: the same call returns different
markup depending on who is asking, it cannot run in a cron job or a unit test,
and the query it hides is invisible to the route that thought it owned the
data. Pass the values in.

**Escaping mistakes.** Three shapes recur: concatenating a user value into
markup and marking it `.raw`; adding `.safe` inside `{{ }}` (which double
escapes); and building a URL from an unvalidated parameter. The first is XSS,
the second mangles text. Interpolate plain values and let `{{ }}` escape them.

**Giant methods.** A method that renders a whole page with five nested loops is
a template class in disguise. Split it by region, one method per block, and let
each method fit on a screen.

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
</main>)
```

Semantic HTML improves accessibility (screen readers understand page structure),
SEO (crawlers parse meaningful sections), and readability (future you will
thank you).

### Pitfall

**Same-tag nesting still needs matching close tags.** The outermost tag of an
inline HTML string may repeat at any depth, but every copy has to be closed —
the string ends only at the close tag that brings the outermost one back to
zero:

```wren
// Wrong — the second <section> is never closed
var bad = <section><section>Nested section</section>

// Correct — close the nested copy too
var good = <section><section>Nested section</section></section>
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

### No CSS by default

Bialet outputs plain HTML and ships no stylesheet. Keep it that way unless the
page needs presentation: semantic elements are readable on their own, and a
small hand-written `style.css` is easier to keep than a framework.

When you do want one, classless CSS frameworks fit the semantic markup best —
**µCSS** (<https://mucss.org/>) styles the elements with no classes:

```wren
head { <head>
  <title>{{ _title }}</title>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <link rel="stylesheet" href="https://unpkg.com/@digicreon/mucss/dist/mu.fuchsia.css" />
</head> }
```

Swap `mu.fuchsia.css` for another theme (`mu.blue.css`, `mu.amber.css`, ...).
[Bulma](https://bulma.io/) is the class-based alternative when you want prebuilt
components; Tailwind and plain CSS work too.

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

- **Tag nesting:** Any tag may contain any other tag, including its own name —
  `<div><div>...</div></div>` compiles. Every copy of the outermost tag must
  still be closed: a missing close tag or an extra one is a compile error. A
  close tag with a different name is not validated, so it only fails when it
  leaves the outermost tag one close short.
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
- **Map callback is a single expression:** An HTML body may start on the next
  line; a code body must start on the `{` line, or it becomes a statement body
  whose value is discarded (warned inside `{{ }}`, silent elsewhere).
- **Empty lists in `map`:** Produce empty output — pair with `&&` for empty
  states.
- **`return` terminates immediately:** Code after `return` never executes.
- **DB values are strings:** Convert with `Num.fromString()` before math.
- **Forgetting `return` before `Response.redirect()`:** The code after it
  still runs and attaches a body to the 302 — no error, just a redirect with
  an unexpected body. Always `return Response.redirect(...)`.
- **Instance vs static methods:** Instance methods need `new()`; static methods
  don't.
- **Repeated markup:** Components are classes with methods that return HTML.
  A card, status bar or navigation block repeated in two pages belongs in one
  method, not in both files.
- **Components reading the request:** Pass data as parameters. A component that
  reads `Request` or `Session` cannot be reused or unit-tested.
- **HTML as a call argument:** Start the literal on a new line. After a comma
  the lexer reads `<` as less-than, so `Layout.page("X", null, <main>...)` does
  not compile; `Layout.page("X", null,` followed by the literal on the next line
  does.
- **`_`-prefixed CSS is not served:** Name stylesheets without leading `_`.
- **CSS preprocessors not built in:** Write vanilla CSS or use a build step.
- **Framework CDNs in production:** Self-host CSS files for reliability.
- **`{{ }}` escapes by default:** Plain values are auto-escaped; do not add
  `.safe` inside `{{ }}` (it double-escapes). Use `HtmlNode` or `.raw` for
  intentionally-raw markup.
