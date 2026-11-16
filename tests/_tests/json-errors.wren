// Json.parse rejects malformed input with a catchable error instead of
// returning a half-parsed value or crashing the worker.
var fails = Fn.new { |text|
  var fiber = Fiber.new { Json.parse(text) }
  fiber.try()
  return fiber.error != null
}

Test.assert(fails.call(""), "empty input is rejected")
Test.assert(fails.call("   "), "whitespace-only input is rejected")
Test.assert(fails.call("\"unterminated"), "unterminated string is rejected")
Test.assert(fails.call("\"bad\\q\""), "unknown escape is rejected")
Test.assert(fails.call("\"\\u00\""), "short unicode escape is rejected")
Test.assert(fails.call("\"\\uZZZZ\""), "non-hex unicode escape is rejected")
Test.assert(fails.call("\"\\uD800\""), "lone high surrogate is rejected")
Test.assert(fails.call("\"\\uDC00\""), "lone low surrogate is rejected")
Test.assert(fails.call("\"\\uD800\\u0041\""), "high surrogate without low is rejected")
Test.assert(fails.call("[1,]"), "trailing comma in array is rejected")
Test.assert(fails.call("{\"a\":1,}"), "trailing comma in object is rejected")
Test.assert(fails.call("{\"a\":}"), "missing object value is rejected")
Test.assert(fails.call("{\"a\" 1}"), "missing object colon is rejected")
Test.assert(fails.call("[1 2]"), "missing array comma is rejected")
Test.assert(fails.call("tru"), "truncated literal is rejected")
Test.assert(fails.call("1 2"), "trailing data is rejected")
Test.assert(fails.call("-"), "lone minus is rejected")
Test.assert(fails.call("1e"), "exponent without digits is rejected")
Test.assert(fails.call("{\"a\":1}x"), "trailing garbage is rejected")
Test.assert(fails.call("[1, 2"), "unclosed array is rejected")
Test.assert(fails.call("{\"a\": 1"), "unclosed object is rejected")

// The parser stops at MAX_JSON_DEPTH (128) instead of recursing to a stack
// overflow.
var deep = ""
for (i in 0...200) deep = deep + "["
Test.assert(fails.call(deep), "deeply nested input is rejected")

// Valid but awkward input still parses.
Test.assert(Json.parse("\"\\u0000\"") == "\u0000", "NUL escape is preserved")
Test.assert(Json.parse("\"\\uD83D\\uDE00\"") == "😀", "surrogate pair is combined")
Test.assert(Json.parse("[1,2,3]").count == 3, "array still parses")
Test.assert(Json.parse("{\"a\":{\"b\":null}}")["a"]["b"] == null, "nested null still parses")
Test.assert(Json.parse("0.5") == 0.5, "decimal still parses")

// The argument must be a string: the primitive is reached through
// Json.parse(_) and rejects anything else before scanning.
Test.assert(fails.call(42), "a number is rejected")
Test.assert(fails.call(null), "null is rejected")
Test.assert(fails.call([]), "a list is rejected")

// A number whose magnitude overflows a double is rejected instead of being
// silently turned into infinity.
Test.assert(fails.call("1e999"), "overflowing number is rejected")

// Literals with the right first byte but the wrong tail, and a bad escape in
// an object key, must be reported as parse errors.
Test.assert(fails.call("falze"), "bad false literal is rejected")
Test.assert(fails.call("nul"), "bad null literal is rejected")
Test.assert(fails.call("{\"\\uZZZZ\":1}"), "bad escape in a key is rejected")

// Escapes above the 2-byte UTF-8 range encode as 3 bytes.
Test.assert(Json.parse("\"\\u4e2d\"") == "中", "CJK escape encodes as 3 bytes")
Test.assert(Json.parse("\"\\u20AC\"") == "€", "euro sign encodes as 3 bytes")
