// A '<' in prose is not a tag: it must stay text even without a matching '>'
// before the closing tag, and comparisons keep working.
var prose = <p>a<b c</p>
var compare = <p>1 < 2 and 3 > 2</p>

return <main>{{ prose }}{{ compare }}</main>
