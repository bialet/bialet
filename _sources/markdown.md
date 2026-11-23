# Markdown

Bialet renders Markdown to HTML with a built-in parser. No external library,
no bundler — the renderer ships inside the binary.

## Rendering Markdown

Four methods on the `Markdown` class:

```wren
// Render a Markdown string
var content = Markdown.html("## Hello **World**!")

// Read a .md file from the app directory and render it
var content = Markdown.file("about.md")

// List the public Markdown files in a directory
var names = Markdown.list("cards")

// Read the front matter of a .md file as a Map
var meta = Markdown.meta("cards/01.md")
```

Use the result anywhere a string works — interpolate it into a page:

```wren
return <main>
  {{ Markdown.file("docs/readme.md") }}
</main>
```

The full API reference for every method lives in the
[Reference](reference.md) under "Markdown".

## Listing and Metadata

`Markdown.list(dir)` returns a sorted `List` of bare filenames in `dir`, relative
to the app root. `""` means the root itself. Only `.md` and `.markdown` files
(case-insensitive) are returned, and names starting with `_` or `.` are skipped:

```wren
for (name in Markdown.list("cards")) {
  System.print(name)          // "01.md", "02.md", ...
}
```

A missing, empty or invalid directory returns `[]`. The directory must stay
inside the app root, and no path component may start with `_` or `.`.
`Markdown.list("../")` and `Markdown.list("_private")` both return `[]`.
Symlinks and non-regular files are not listed and are never followed.

`Markdown.meta(file)` reads leading YAML-ish front matter and returns a
`Map` of `String` to `String`:

```markdown
---
title: The Coral Path
category: "adventure"
status: 'draft'
---
# The Coral Path
```

```wren
var meta = Markdown.meta("cards/01.md")
meta["title"]      // "The Coral Path"
meta["category"]   // "adventure" (quotes stripped)
```

The opener and closer must each be a line containing only `---`. Keys and values
are trimmed, one matching pair of single or double quotes around a value is
removed, and blank lines and lines starting with `#` are ignored. Files without
front matter return `{}`; a missing or unreadable file returns `false`.

Front matter is **flat**: nested keys, lists, and multi-line values are not
parsed. A line without a `:` is ignored, and a later key overwrites an earlier
one with the same name. `Markdown.meta` applies the same `.md`-only, app-root and
private-name rules as `Markdown.file`.

> ⚠️ Pitfall: `Markdown.file` still skips front matter. It is metadata, not
> content, and `Markdown.meta` is the only way to read it.

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
and `Markdown.file("page.wren")` all return `false`. `Markdown.list()` and
`Markdown.meta()` apply the same rules. That keeps
`Markdown.file(Request.get("page"))` from handing out private files, config or
route source, but still prefer an allow-list of page names over a raw request
value.

**`-i`/`--ignore` is a serving policy, not a Wren read boundary.** The ignore
list only decides which files the HTTP server will serve. It does **not**
restrict `Markdown.file()`, `Markdown.list()` or `Markdown.meta()`: a
`README.md` or `AGENTS.md` is ignored over HTTP but still readable and listable
from Wren. Do not put secrets in a public-named Markdown file. Name it with a
leading `_` or `.`, or pass only values from an allow-list to the Markdown API.

**Front matter is skipped by `Markdown.file()`, not parsed into the output.** A
leading `---` block is dropped from the rendered HTML. Use `Markdown.meta()` to
read its keys; the renderer never includes them.

**Converting the rendered HTML to a `String` loses its safe marker.**
`Markdown.file()` returns an `HtmlNode`, which `{{ }}` inserts verbatim. As soon
as you turn it into a plain `String` — with `.toString`, `"%(...)"`, or
`.split(...)` — the value is ordinary text again and `{{ }}` escapes its tags.
Re-mark the piece you want to render with `.raw`:

```wren
// Split a post at <!--more--> and render the first part inline
var chunks = Markdown.file("post.md").toString.split("<!--more-->")

return <article>
  {{ chunks[0] }}
  <a href="/post">Read more</a>
  {{ chunks[1].raw }}
</article>
```

Without `.raw`, `chunks[1]` renders as visible `<p>` tags. `.raw` asserts the
string is already safe HTML, so only do this to markup you produced yourself or
trust (see [Security](#security)).

