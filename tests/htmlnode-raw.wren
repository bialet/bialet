// HtmlNode is trusted markup: `{{ }}` renders it verbatim, while a plain
// string in the same position is escaped.
var plain = "<b>x</b>"
var node = HtmlNode.new(plain)
return <div><p>{{ node }}</p><p>{{ plain }}</p></div>
