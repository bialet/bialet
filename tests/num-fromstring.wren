// A missing form field or a NULL column arrives as null. Num.fromString must
// return null, the same as an unparseable string, instead of raising.
var nullValue = Num.fromString(null)
var badValue = Num.fromString("nope")
var goodValue = Num.fromString("42")

return "null:" + (nullValue == null ? "null" : nullValue.toString) +
  "|bad:" + (badValue == null ? "null" : badValue.toString) +
  "|good:" + goodValue.toString
