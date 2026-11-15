// An interpolation inside the attribute list of a nested copy of the outermost
// tag: its expression may hold a quote or a '<', which must not be read as
// markup around the tag.
var quoted = <div><div style="width:{{ "</b>" }}">a</div><div>b</div></div>
var compared = <div><div title={{ 1 < 2 }}>x</div></div>

// The same shapes in the outermost tag's own attributes.
var outerQuoted = <div style="{{ "</div>" }}">y</div>

return <main>{{ quoted }}{{ compared }}{{ outerQuoted }}</main>
