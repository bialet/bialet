# 4. Templates

To avoid repeating HTML across pages, we'll create a shared layout.

Create `_app.wren` — a special file that Bialet loads automatically:

```wren
class Template {
  static layout(content) {
    return <html>
      <head>
        <title>Poll</title>
      </head>
      <body>
        <header>
          <h1><a href=".">Poll</a></h1>
        </header>
        <main>{{ content }}</main>
      </body>
    </html>
  }
}
```

- Files starting with `_` are **private** (not served directly) but can be
  imported
- `_app.wren` is automatically available to all pages in your app
- `static` methods can be called without creating an instance

Now use the template in `index.wren`:

```wren
import "_app" for Template

var html = Template.layout("So far, so good.")
return html
```

`return` sets the response body. `Template.layout()` wraps
content in the shared HTML shell.

## Refining the template

Inline HTML Strings support multi-line and single-expression returns. We can
simplify:

```wren
class Template {
  static layout(content) { <html>
    <head><title>Poll</title></head>
    <body>
      {{ header }}
      <main>{{ content }}</main>
    </body>
  </html> }

  static header { <header><h1><a href=".">Poll</a></h1></header> }
}
```

A Wren block with a single expression implicitly returns that expression's
value — no `return` keyword needed.

## Copy the vote page

Now copy the HTML from `vote.html` (from the introduction) into
`Template.layout()` in `index.wren`. Create a `results.wren` by doing the
same with `results.html`.

The starting files for this step:
- [\_app.wren](2-no-logic/_app.wren)
- [index.wren](2-no-logic/index.wren)
- [results.wren](2-no-logic/results.wren)

We've replicated the static HTML. Next, we'll make it dynamic with a
database.

---
**Previous:** [3. Your First Page](3-first-page) &nbsp; | &nbsp; **Next:** [5. Logic & Database](5-logic-database)
