# Markdown

Bialet renders Markdown to HTML with a built-in parser. No external library,
no bundler — the renderer ships inside the binary.

## Rendering Markdown

Two methods on the `Markdown` class:

```wren
// Render a Markdown string
var content = Markdown.html("## Hello **World**!")

// Read a .md file from the app directory and render it
var content = Markdown.file("about.md")
```

Use the result anywhere a string works — interpolate it into a page:

```wren
return <main>
  {{ Markdown.file("docs/readme.md") }}
</main>
```

The full API reference for both methods lives in the
[Reference](reference.md) under "Markdown".

## Supported Syntax

The parser implements a focused subset of Markdown:

| Syntax | Renders to |
|---|---|
| `#` to `######` headings (must be followed by a space) | `<h1>` to `<h6>` |
| `**bold**` and `*italic*` | `<strong>` / `<em>` |
| `` `inline code` `` | `<code>` (escaped) |
| Fenced code blocks with ` ``` ` | `<pre><code>` (escaped) |
| `[text](url)` links | `<a href="url">` |
| `![alt](url)` images | `<img alt="..." src="url">` |
| `- item` / `* item` unordered lists | `<ul><li>` |
| `1. item` ordered lists | `<ol><li>` |
| `> quote` blockquotes | `<blockquote>` |
| `\| a \| b \|` tables | `<table><th><td>` |
| Leading `---` front matter | skipped |

CommonMark and GitHub Flavored Markdown are **not** fully supported. Syntax
outside this list (footnotes, task lists, strikethrough, nested lists) is not
rendered — it comes through as plain text. Test your Markdown against the
renderer before relying on it.

## Security

The parser escapes HTML **only inside code blocks and inline code**. Raw HTML
in paragraphs, headings, and lists passes through to the output unescaped.
Link and image URLs are not validated either.

> ⚠️ Pitfall: rendering untrusted input with `Markdown.html()` can produce
> XSS. `Markdown.html("<script>alert(1)</script>")` emits a live `<script>`
> tag, and `[x](javascript:alert(1))` emits a `javascript:` link. Render only
> Markdown you trust, or sanitize the output before serving it.

## Pitfalls

**`Markdown.file()` returns `false` when the file is missing**, it does not
throw. Guard against it:

```wren
var content = Markdown.file("about.md")
if (content == false) content = "<p>About page not written yet.</p>"
```

**Output is capped at 2 MB.** Larger documents are truncated silently.

**`_`-prefixed files are private.** A `_about.md` file renders 403 when fetched
directly, but `Markdown.file("_about.md")` still reads it from disk — the
privacy rule applies to HTTP requests, not file reads.

**Front matter is skipped, not parsed.** A leading `---` block is dropped from
the output. Metadata inside it (title, date, tags) is not exposed — you must
read it yourself if you need it.
