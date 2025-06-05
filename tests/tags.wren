
var a = "A"
var b = "B"
var n = 1
var m = 2
var i = Num.fromString(Request.get("i") || "1")
var j = Num.fromString(Request.get("j") || "2")
var list = [1, 2, 3]

var singleStaticTag = <span>A</span>
var emptyTag = <div></div>
var selfClosingTag = <hr />
var interpolated = <p>{{ singleStaticTag }}</p>
var selfClosingInterpolated = <img src="{{ a }}" />
var attributes = <a href="#">A</a>
var selfClosingAttributes = <input name="i" />
var multiline = <p>
<span>B</span>
</p>
var interpolatedAttributes = <p id="{{ i }}_{{ j }}">C</p>
var interpolatedInsideInterpolated = <ul>{{ list.map{|x| <li>{{ x }}</li> } }}</ul>
var conditionalRendering = <p>{{ a != b && "C" }}</p>
var ternary = <p>{{ a == b ? "<b>==</b>" : "<b>!=</b>" }}</p>
var content = ""

// Yes, < and > signs still work!
if (n<m || m>n) {
  content = emptyTag + selfClosingTag + interpolated + selfClosingInterpolated + attributes + selfClosingAttributes + multiline + interpolatedAttributes + interpolatedInsideInterpolated + ternary + conditionalRendering
}

class Template {
  static section {
    return <section>
Lorem ipsum
</section>
  }
  static form(a, b) { <form method="POST" class="{{ a }}">
<p>
<input name="b" value="{{ b }}">
<label for="b">B</label>
</p>
</form> }
  static html { section + form("a", "b") }
}

// Doctype is a special case, it will end on a close HTML tag: `</html>`
Response.out(<!doctype html><html><body>{{ content }}{{ Template.html }}</body></html>)
