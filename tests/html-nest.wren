// Same-tag nesting in inline HTML strings. The outermost tag may appear as a
// child at any level; the literal ends at the close tag that matches it.
var inner = <div class="inner">child</div>
var two = <div><div>hola</div></div>
var three = <div><div><div>deep</div>middle</div>outer</div>
var siblings = <div><div>one</div><div>two</div></div>
var attributed = <div class="outer"><div class="inner">x</div></div>
var multiline = <div>
  <div>
    <div>deep</div>
  </div>
</div>
var interpolated = <div>{{ 1 + 1 }}<div>{{ "two" }}</div></div>
var interpolatedAttribute = <div class="{{ "o" }}"><div class="i">y</div></div>
var interpolatedNestedAttribute = <div><div style="width:{{ 7 }}px">w</div><div>v</div></div>
var literalInsideInterpolation = <div>{{ true && <div>z</div> }}</div>
var literalInBlock = <div>{{ [1, 2].map{|n| <div>{{ n }}</div> } }}</div>
var attributeMarkup = <div title="<div>">literal</div>
var voidChild = <div>line<br />two</div>
var comparisons = <p>{{ 1 < 2 }} and 2 < 3</p>

return <main>{{ inner }}{{ two }}{{ three }}{{ siblings }}{{ attributed }}{{ multiline }}{{ interpolated }}{{ interpolatedAttribute }}{{ interpolatedNestedAttribute }}{{ literalInsideInterpolation }}{{ literalInBlock }}{{ attributeMarkup }}{{ voidChild }}{{ comparisons }}</main>
