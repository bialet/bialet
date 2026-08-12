# JSX to Bialet Migration

You are moving a React/JSX codebase to Bialet. The good news: inline HTML
strings look a lot like JSX. The bad news: the surface similarity hides hard
limits that will bite you on the first nested list. This page is the
cheatsheet — the five differences that turn JSX code into Bialet code.

The short version: inline HTML strings are **plain strings**, not a component
tree. No props, no children, no expressions-in-markup outside `{{ }}`, and no
`dangerouslySetInnerHTML`.

---

## 1. Tag Nesting Limitations

JSX lets you nest any element inside any element. Bialet has one hard rule:

**The outermost tag of an inline HTML string must not appear again as a
direct child tag at any nesting level.**

The parser checks the whole string, not just the first level, and reports
`Cannot nest <div> inside <div>` on the offending line.

```wren
// WRONG — <div> is the outermost tag AND a child
var bad = <div><div>Hello</div></div>
var alsoBad = <section><section>Nested</section></section>

// CORRECT — open with a different tag, then nest freely
var good = <div><section>Hello</section></div>

// CORRECT — once the tree starts with a different tag, the
// original tag can appear at any depth below it
var fine = <section>
  <div>
    <div>Deeply nested same tags work here</div>
  </div>
</section>
```

**Pitfall:** this bites the common JSX pattern of a wrapper `<div>` inside a
`<div>` (row, then column). If you need the same tag twice, change the
outermost wrapper or extract the inner one into a component method (see
section 4).

Two more parser quirks to unlearn from JSX:

- **Mismatched closing tags are not validated.** Only the outermost closing
  tag is matched. `<div><span>Hello</div>` compiles and is served verbatim —
  the browser auto-closes the `<span>` at render time. Write well-formed HTML
  yourself; there is no compiler checking your balance.
- **Tag names are restricted.** Lowercase, start with a letter, only letters,
  numbers, and hyphens. `<MyComponent>` and `<my_component>` are compiler
  errors. PascalCase component names are a JSX thing — see section 4.

---

## 2. Map Callback Restrictions

In JSX you write `{items.map(item => <li>{item}</li>)}` and the arrow function
can hold any logic. In Bialet the `map` callback is a **single Wren
expression** — the inline HTML string itself:

```wren
var items = ["Apple", "Banana", "Cherry"]
var html = <ul>{{ items.map { |item| <li>{{ item }}</li> } }}</ul>
// Output: <ul><li>Apple</li><li>Banana</li><li>Cherry</li></ul>
```

The two restrictions that will trip you up:

### No statements inside the callback

You cannot declare a variable or write multiple lines of Wren inside `map`:

```wren
// WRONG — multiple statements inside the map callback
{{ tasks.map { |task|
  var icon = task.finished ? "✓" : "○"
  <li>{{ icon }} {{ task.description }}</li>
} }}
```

If you need pre-computed values, compute them **before** `map`, or add a
computed property to your model class:

```wren
var tasks = Tasks.list().map { |t| t.withIcon }   // pre-process the list
return <ul>{{ tasks.map { |t| <li>{{ t.icon }} {{ t.description }}</li> } }}</ul>
```

### The body must start on the line after `{`

A multi-line block whose body is not a single expression on the `{` line
returns `null` silently — the output is empty and nothing tells you why:

```wren
// WRONG — the callback body breaks onto its own line, returns null, renders empty
return <div>{{ items.map { |item|
  <li>{{ item }}</li>
} }}</div>

// CORRECT — the expression starts on the {{ line; the HTML string
// itself may span many lines
return <div>{{ items.map { |item| <li>
  {{ item.description }}
</li> } }}</div>
```

### Empty lists render empty

`map` over an empty list produces nothing. In JSX you'd conditionally render a
fallback; in Bialet you pair `map` with `&&`:

```wren
return <main>
  <ul>
    {{ tasks.map { |task| <li>{{ task.description }}</li> } }}
  </ul>
  {{ tasks.count == 0 && <section class="empty-state">
    <p>No tasks yet.</p>
  </section> }}
</main>
```

---

## 3. Interpolation Rules

JSX interpolates with `{expr}`. Bialet uses `{{ expr }}` — the extra brace is
what makes it valid Wren inside an HTML string. The rules differ more than the
syntax:

### `{{ }}` is the only interpolation point

Interpolation happens only inside `{{ }}`. A bare `{expr}` is not a template
expression — inside an HTML string it is literal text, and outside one it is
Wren map syntax.

### Values are escaped by default

This is the biggest safety win over JSX (where you must remember
`dangerouslySetInnerHTML`). Plain values — user input, database rows, URL
parameters — are HTML-escaped automatically:

```wren
var userInput = "<script>alert('xss')</script>"
var safe = <p>{{ userInput }}</p>
// Renders: <p>&lt;script&gt;alert(&#x27;xss&#x27;)&lt;/script&gt;</p>
```

### Line breaks follow Wren, not JSX

A `{{ }}` expression can span lines, but an infix operator must end its line.
A leading operator is a parse error:

```wren
// CORRECT — operators end each line
return <div>{{
  showClear &&
  <form method="post" action="/clear">
    <button>Clear</button>
  </form>
}}</div>

// WRONG — the operator starts a line
return <div>{{
  showClear
  && <form method="post" action="/clear">
    <button>Clear</button>
  </form>
}}</div>
```

### Interpolation depth is limited

Nesting is capped at 9 levels of `{{ }}`. Deeply nested JSX interpolations
must be flattened.

### No expressions in string values

JSX lets you write `className={`btn ${active ? 'on' : ''}`}` — a template
literal inside an attribute. Bialet does not support string interpolation
into attribute values. Compute the string first, then interpolate the result:

```wren
// WRONG — no template literals
<a className="btn {{ active ? 'on' : '' }}">...</a>

// CORRECT — compute the class, then interpolate it
var cls = "btn " + (active ? "on" : "")
<a class="{{ cls }}">...</a>
```

For simple two-way choices, the ternary inside `{{ }}` is fine:

```wren
<button class="{{ task.finished ? "checkbox-btn checked" : "checkbox-btn" }}">
```

---

## 4. Component-as-Method Pattern

JSX has components: functions that take props and return JSX, composed via
`<Card title="x" />`. Bialet has **methods that return inline HTML strings**.
There is no prop syntax, no children slot, no component invocation in markup.

Define a component as a method on a class:

```wren
// _template.wren
class Template {
  card(title, body) { <article class="card">
    <h2>{{ title }}</h2>
    <div class="card-body">{{ body }}</div>
  </article> }

  layout(content) { <!doctype html>
    <html lang="en">
      <body>
        <main>{{ content }}</main>
      </body>
    </html> }
}
```

Compose by passing markup into methods, not by nesting tags:

```wren
// index.wren
import "_template" for Template

return Template.new().layout(
  Template.new().card("Hello", <p>World</p>)
)
```

Because the method returns a plain string, the same-tag nesting rule applies
across the boundary: if a component's outermost tag would repeat under a
caller's outermost tag, the parser rejects it. `card()` returning an
`<article>` inside a page whose outermost tag is also `<article>` fails —
widen one of them.

**Pitfall:** instance methods need `new()`. `Template.layout(...)` looks for
a static method and fails. Always instantiate first.

**Pitfall:** a method body is an expression body only when the expression
starts on the same line as `{`. A statement body on the following line
returns `null` silently — the component renders as empty with no error.
Keep the opening HTML tag on the method's first line.

---

## 5. Raw HTML Security Footguns

Bialet escapes by default — the safety flip of JSX's default-raw model. The
footguns are the ways you can accidentally defeat that default and re-open
the XSS holes `dangerouslySetInnerHTML` created for you.

### Footgun 1: `.safe` is the opposite of what you expect

`.safe` marks a string as trusted markup. Inside `{{ }}`, interpolation
already escapes, so `{{ userInput.safe }}` escapes the text **twice**
(`&amp;lt;` instead of `&lt;`) — the raw HTML you meant is shown as text.
Drop `.safe` from templates entirely.

### Footgun 2: `.raw` / `HtmlNode` skip all sanitization

To render a runtime string as markup you must mark it safe, and that is an
assertion, not a sanitizer:

```wren
var html = "<b>bold</b>".raw
<p>{{ html }}</p>            // rendered as written
<p>{{ "<b>bold</b>" }}</p>   // escaped: &lt;b&gt;bold&lt;/b&gt;
```

`.raw`, `HtmlNode.new(...)`, and `HtmlNode` values are inserted verbatim —
no escaping, no scrubbing of `<script>`, no filtering of `javascript:` URLs.
Never use them on untrusted input. The JSX instinct "I need raw HTML here"
should map to "this value is markup I fully control", and nothing else.

### Footgun 3: `Markdown.html()` passes raw HTML through

The built-in Markdown parser escapes HTML only inside code blocks. Raw HTML
in paragraphs, headings, and lists passes through unescaped, and link URLs
are not validated. Rendering untrusted Markdown is XSS:

```wren
// WRONG — raw HTML passes through to the output
var content = Markdown.html("<script>alert('xss')</script>")

// WRONG — javascript: links are not filtered
var link = Markdown.html("[x](javascript:alert('xss'))")
```

Render only Markdown you trust.

### Footgun 4: interpolation inside `<script>` and `href` is not "safe"

Escaping makes `{{ value }}` safe for text and attribute values, but it does
not neutralize JavaScript or URLs. Never interpolate untrusted input into a
`<script>` block or a `javascript:` href — restructure the page so it cannot
happen.

### Footgun 5: `Request.post()` returns `null`

Reading a missing form field gives you `null`, not `""`. String operations on
it crash the request. Guard every read:

```wren
var msg = Request.post("msg") || ""
```

---

## Migration checklist

- [ ] Replace `{expr}` with `{{ expr }}` — and expect values to be escaped.
- [ ] Open repeated same-tag nesting with a different outer tag, or extract a
      component method.
- [ ] Move statements out of `map` callbacks; keep the callback body a single
      expression starting on the `{` line.
- [ ] Compute strings before interpolating into attributes; no template
      literals.
- [ ] Turn components into class methods; compose by passing markup, not by
      nesting tags.
- [ ] Replace `dangerouslySetInnerHTML` with `.raw`/`HtmlNode` only for markup
      you fully control; never for user input.
- [ ] Guard `Request.post()` with `|| ""`.

See [Template](template.md) for the full inline-HTML reference and
[Security](security.md) for the escape rules and pitfalls.
