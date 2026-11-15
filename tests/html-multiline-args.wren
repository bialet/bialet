// A call whose HTML argument is followed by a newline must stay a single call:
// the interpolated literal is lexed in pieces, and the whitespace after its
// last piece has to be skipped exactly like a literal without interpolation.
class Layout {
  static render(content, title) { <section data-title="{{ title }}">{{ content }}</section> }
}

var x = 1

// The shape that used to fail: comma on the next line after the literal.
var multiline = Layout.render(
  <div><div>{{ x }}</div></div>
, "Titulo")

// Same call, one line, no interpolation.
var singleLine = Layout.render(<div><div>1</div></div>, "Titulo")

// Closing parenthesis on its own line.
var closingParen = Layout.render(
  <div><div>{{ x }}</div></div>
, "Titulo"
)

// Only the argument order can differ, so all three must render the same HTML.
var same = multiline.toString == singleLine.toString &&
           multiline.toString == closingParen.toString
if (!same) return "different:" + multiline + "|" + singleLine + "|" + closingParen
return "three:same"
