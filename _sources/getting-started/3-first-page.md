# 3. Your First Page

Create `index.wren` in your project directory:

```wren
return <p>Hello World!</p>
```

Refresh [127.0.0.1:7001](http://127.0.0.1:7001). You should see "Hello
World!"

## How it works

- `index.wren` is the default page, just like `index.html`
- `return` finishes the script and sends the response
- `<p>Hello World!</p>` is an **Inline HTML String** — you write HTML
  directly in Wren

Inline HTML Strings work like JSX in React, but produce plain strings
instead of objects. You can embed Wren expressions with `{{ }}`:

```wren
var name = "Bialet"
return <h1>Hello {{ name }}!</h1>
```

Everything inside `{{ }}` is evaluated as Wren code and the result is
inserted into the HTML. You can use variables, conditionals, loops — any
valid Wren expression.

```wren
var items = ["apples", "oranges", "bananas"]
return <ul>
  {{ items.map {|item| <li>{{ item }}</li> } }}
</ul>
```

For more details, see [Inline HTML Strings](../html-strings.md).

---
**Previous:** [2. Setup](2-setup) &nbsp; | &nbsp; **Next:** [4. Templates](4-templates)
