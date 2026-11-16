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
