// A link with a script-capable scheme must not become an href, and markup
// inside code spans is escaped. Raw HTML in a paragraph passes through by
// design (documented limitation).
var links = Markdown.html("[click](javascript:alert(1))").toString
var code = Markdown.html("`<b>`").toString
var raw = Markdown.html("<b>raw</b>").toString
return "links:%(links)|code:%(code)|raw:%(raw)"
