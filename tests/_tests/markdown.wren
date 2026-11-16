// The documented syntax subset, asserted on rendered output.
Test.assert(Markdown.html("# H1").toString.contains("<h1>H1</h1>"), "markdown h1")
Test.assert(Markdown.html("###### H6").toString.contains("<h6>H6</h6>"), "markdown h6")
Test.assert(Markdown.html("plain text").toString.contains("<p>plain text</p>"), "markdown paragraph")
Test.assert(Markdown.html("**bold**").toString.contains("<strong>bold</strong>"), "markdown strong")
Test.assert(Markdown.html("*italic*").toString.contains("<em>italic</em>"), "markdown em")
Test.assert(Markdown.html("`a < b`").toString.contains("<code>a &lt; b</code>"), "markdown code is escaped")
Test.assert(Markdown.html("- one\n- two").toString.contains("<li>one</li>"), "markdown unordered list")
Test.assert(Markdown.html("1. one\n2. two").toString.contains("<ol>"), "markdown ordered list")
Test.assert(Markdown.html("> quoted").toString == "<blockquote>\nquoted<br>\n</blockquote>\n", "markdown blockquote")
Test.assert(Markdown.html("[x](https://example.com)").toString.contains("<a href=\"https://example.com\">x</a>"),
            "markdown link")
Test.assert(Markdown.html("![alt](https://example.com/a.png)").toString.contains("<img alt=\"alt\" src=\"https://example.com/a.png\">"),
            "markdown image")
Test.assert(Markdown.html("| a | b |\n| - | - |\n| 1 | 2 |").toString.contains("<table>"), "markdown table")
Test.assert(!Markdown.html("---\ntitle: t\n---\n\n# Body").toString.contains("title:"), "markdown skips front matter")

// A script-capable scheme never becomes an href. Parentheses inside the URL do
// not leak trailing text into the output.
var unsafe = Markdown.html("[x](javascript:alert(1))").toString
Test.assert(!unsafe.contains("javascript:"), "markdown drops a javascript: link")
Test.assert(!unsafe.contains("href"), "markdown emits no href for an unsafe scheme")
Test.assert(unsafe.contains(">x<"), "markdown keeps the link label")
Test.assert(!unsafe.contains(")</p>"), "markdown does not leak the URL's closing paren")

var unsafeImage = Markdown.html("![alt](javascript:alert(1))").toString
Test.assert(!unsafeImage.contains("<img"), "markdown drops an unsafe image")
Test.assert(unsafeImage.contains("alt"), "markdown keeps the image alt text")

// A safe link with nested parentheses must render the whole URL.
var nested = Markdown.html("[wiki](https://en.wikipedia.org/wiki/Foo_(bar))").toString
Test.assert(nested.contains("href=\"https://en.wikipedia.org/wiki/Foo_(bar)\""),
            "markdown keeps nested parens in a URL")

// Raw HTML in a paragraph passes through by design (documented limitation).
Test.assert(Markdown.html("<b>raw</b>").toString.contains("<b>raw</b>"), "markdown passes raw html")
Test.assert(Markdown.file("no-such-file.md") == false, "Markdown.file missing returns false")
