// Json.stringify escapes the characters a JSON string cannot carry raw.
Test.assert(Json.stringify(null) == "null", "stringify null")
Test.assert(Json.stringify(true) == "true", "stringify true")
Test.assert(Json.stringify(42) == "42", "stringify number")
Test.assert(Json.stringify("plain") == "\"plain\"", "stringify string")
Test.assert(Json.stringify("a\"b") == "\"a\\\"b\"", "stringify escapes a quote")
Test.assert(Json.stringify("a\\b") == "\"a\\\\b\"", "stringify escapes a backslash")
Test.assert(Json.stringify("a\nb") == "\"a\\nb\"", "stringify escapes a newline")
Test.assert(Json.stringify("a\tb") == "\"a\\tb\"", "stringify escapes a tab")
Test.assert(Json.stringify("é") == "\"é\"", "stringify keeps unicode")
Test.assert(Json.stringify([]) == "[]", "stringify empty list")
Test.assert(Json.stringify([1, 2]) == "[1,2]", "stringify list")
Test.assert(Json.stringify({}) == "{}", "stringify empty map")
Test.assert(Json.stringify({"a": 1}) == "{\"a\":1}", "stringify map")
Test.assert(Json.stringify({"a": [1, {"b": null}]}) == "{\"a\":[1,{\"b\":null}]}",
            "stringify nested map and list")

// Round-tripping generated JSON must give the original value back.
var value = {"name": "Ada \"the\" Lovelace", "tags": ["x", "y"], "n": 3.5, "ok": true}
var back = Json.parse(Json.stringify(value))
Test.assert(back["name"] == value["name"], "roundtrip keeps escaped text")
Test.assert(back["tags"][1] == "y", "roundtrip keeps nested lists")
Test.assert(back["n"] == 3.5, "roundtrip keeps numbers")
Test.assert(back["ok"] == true, "roundtrip keeps booleans")
