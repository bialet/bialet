// HtmlNode is the "already safe" wrapper the template engine respects.
var node = HtmlNode.new("<b>x</b>")
Test.assert(node.toString == "<b>x</b>", "HtmlNode.toString returns the raw markup")
Test.assert(node.raw == node, "HtmlNode.raw returns the node itself")

var joined = HtmlNode.new("<b>") + "x"
Test.assert(joined.toString == "<b>x", "HtmlNode + String concatenates raw markup")
Test.assert(joined is HtmlNode, "HtmlNode + returns an HtmlNode")

var fromString = "<i>y</i>".raw
Test.assert(fromString is HtmlNode, "String.raw wraps a string in HtmlNode")
Test.assert(fromString.toString == "<i>y</i>", "String.raw keeps the markup")

// A plain string is not an HtmlNode, which is what makes {{ }} escape it.
Test.assert(!("plain" is HtmlNode), "a plain string is not HtmlNode")
