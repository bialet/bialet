// Regression: a 4-byte sequence above U+10FFFF (here 0xF4 0x90 0x80 0x80, which
// decodes to 0x110000) must be indexed as a raw byte, the same rule the range
// slice applies. wrenStringCodePointAt forwarded the value to
// wrenStringFromCodePoint, which cannot encode it, so s[0] was an empty string
// while s[0..0] held the byte and byte-wise validators read nothing.
var s = "\xf4\x90\x80\x80"
var first = s[0]
var slice = s[0..0]
// Iteration goes through the same helper, so it must not raise either.
var iterated = 0
for (c in s) {
  iterated = iterated + c.bytes.count
}
var same = first.bytes.count == 1 && slice.bytes.count == 1 && first == slice
var byte = first.bytes.count > 0 ? "%(first.bytes[0])" : "none"
return "same:%(same ? "yes" : "no")|byte:%(byte)|iter:%(iterated == 1 ? "yes" : "no")"
