var parts = []
parts.add(Markdown.html("**bold** and *italic*"))
parts.add(Markdown.html("- one\n- two"))
parts.add(Markdown.html("> quoted"))
parts.add(Markdown.html("`a < b`"))
parts.add(Markdown.html("```\n<x>\n```"))
parts.add(Markdown.html("| a | b |\n| - | - |\n| 1 | 2 |"))
parts.add(Markdown.html("---\ntitle: front matter\n---\n\n# Body"))
parts.add(Markdown.html("# H1\n\n## H2"))

// Block transitions: the list type must close before the other one opens.
parts.add(Markdown.html("- a\n1. b\n2. c\n- d"))

// Every character the HTML escaper handles, inside the spans that escape:
// a code span and a link label / image alt.
parts.add(Markdown.html("`a & b \"c\" 'd'`"))
parts.add(Markdown.html("[a & b](/x) ![\"alt\"](y)"))

// Heading levels 3-6, and a fence carrying a language tag.
parts.add(Markdown.html("### h3\n#### h4\n##### h5\n###### h6"))
parts.add(Markdown.html("```wren\nx < y\n```"))

// A table followed by a paragraph closes the table first.
parts.add(Markdown.html("| a | b |\n| - | - |\n| 1 | 2 |\nafter table"))

// Paragraph continuation: indented lines join, a heading without a blank line
// in between still closes the paragraph.
parts.add(Markdown.html("line one\n   line two\n# heading right after"))

// Inline link and image edge cases: relative URLs, an uppercase scheme, a
// rejected scheme, an unterminated link, and an image with no target.
parts.add(Markdown.html("[rel](/a/b) [up](HTTP://Example.COM/x)"))
parts.add(Markdown.html("[bad](htxps:y) [open](z"))
parts.add(Markdown.html("![alt](pic.png) ![dropped](javascript:alert(1)) ![none]"))

// Blockquote with no space after the marker, closed by a blank line.
parts.add(Markdown.html("> tight\n\nafter quote"))

// A leading-indented first line is trimmed before the block is classified.
parts.add(Markdown.html("   spaced paragraph"))

// More than 256 lines forces the line-pointer array in markdown_to_html() to
// grow; the last line has no trailing newline, so it takes the closing branch.
// The lines are long enough that the output buffer grows past its first
// allocation too.
var many = ""
for (i in 1..300) many = many + "line %(i) with some more words to fill it\n"
parts.add(Markdown.html(many))

// An empty document must render as an empty string, not NULL.
parts.add(Markdown.html(""))

return parts.join("\n")
