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
Link and image URLs are checked: only `http`, `https`, `mailto`, `tel`, `ftp`,
relative references and fragments are rendered. A link or image with any other
scheme (including `javascript:`) is dropped, keeping its label or alt text as
plain escaped text.

> ⚠️ Pitfall: rendering untrusted input with `Markdown.html()` can still
> produce XSS, because raw HTML passes through.
> `Markdown.html("<script>alert(1)</script>")` emits a live `<script>` tag.
> Render only Markdown you trust, or sanitize the output before serving it.

## Pitfalls

**`Markdown.file()` returns `false` when the file is missing or not
readable**, it does not throw. Guard against it:

```wren
var content = Markdown.file("about.md")
if (content == false) content = "<p>About page not written yet.</p>"
```

**Output is capped at 2 MB.** Larger documents are truncated silently.

**`Markdown.file()` reads only Markdown inside the app root.** The path must
end in `.md` or `.markdown`, must stay inside the app root, and no component may
start with `_` or `.`: `Markdown.file("_about.md")`, `Markdown.file("config.json")`
and `Markdown.file("page.wren")` all return `false`. That keeps
`Markdown.file(Request.get("page"))` from handing out private files, config or
route source, but still prefer an allow-list of page names over a raw request
value.

**Front matter is skipped, not parsed.** A leading `---` block is dropped from
the output. Metadata inside it (title, date, tags) is not exposed — you must
read it yourself if you need it.
