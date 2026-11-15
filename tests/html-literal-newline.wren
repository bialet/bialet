// A newline after an inline HTML literal ends the statement unless the next
// token continues the expression. The lexer used to skip every newline after
// the literal, so a ternary branch at the end of a line swallowed the break
// and the next statement failed with "Expect newline after statement".
class Layout {
  static render(content, title) { <section data-title="{{ title }}">{{ content }}</section> }
}

var x = 1

// Multiline call: the comma on the next line still keeps the call open.
var multiline = Layout.render(
  <div><div>{{ x }}</div></div>
, "Titulo")

// Interpolated literal as a ternary branch at the end of a line: the newline
// stays a statement break.
var c = false
var badge = c ? "" : <span class="x {{ c && "y" }}">{{ x }}</span>
var plain = c ? "" : <span class="plain">x</span>
var afterBadge = badge.toString
var afterPlain = plain.toString

var ok = multiline.toString.contains("data-title=\"Titulo\"") &&
         afterBadge.contains("<span") && afterPlain.contains("<span") &&
         x == 1
if (!ok) return "bad:" + multiline + "|" + afterBadge + "|" + afterPlain
return "badge:ok"
