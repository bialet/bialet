var results = []

results.add("null:" + (Json.parse("null") == null).toString)
results.add("true:" + (Json.parse("true") == true).toString)
results.add("int:" + (Json.parse("42") == 42).toString)
results.add("str:" + (Json.parse("\"hello\"") == "hello").toString)
results.add("escaped:" + (Json.parse("\"a\\nb\"") == "a\nb").toString)
results.add("unicode:" + (Json.parse("\"\\u00e9\"") == "é").toString)
results.add("arr:" + (Json.parse("[1,2]").count == 2).toString)
results.add("obj:" + (Json.parse("{\"k\":\"v\"}")["k"] == "v").toString)

var nested = Json.parse("{\"a\":{\"b\":[1,2,3]}}")
results.add("nested:" + (nested["a"]["b"][1] == 2).toString)

var invalidResult = false
var fiber = Fiber.new {
  Json.parse("this is not json")
}
var error = fiber.try()
if (fiber.error != null) {
  invalidResult = true
}
results.add("invalid:" + invalidResult.toString)

var allPassed = true
for (r in results) {
  if (!r.endsWith("true")) {
    allPassed = false
  }
}

if (allPassed) {
  Response.out("all-passed")
} else {
  Response.out(results.join(","))
}
