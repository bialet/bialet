Test.assert(Json.parse("null") == null, "parse null")
Test.assert(Json.parse("true") == true, "parse true")
Test.assert(Json.parse("false") == false, "parse false")
Test.assert(Json.parse("0") == 0, "parse zero")
Test.assert(Json.parse("-1") == -1, "parse negative int")
Test.assert(Json.parse("42") == 42, "parse positive int")
Test.assert(Json.parse("3.14") == 3.14, "parse float")
Test.assert(Json.parse("-0.5") == -0.5, "parse negative float")
Test.assert(Json.parse("1e10") == 1e10, "parse scientific notation")
Test.assert(Json.parse("2.5e-3") == 2.5e-3, "parse scientific negative exp")

Test.assert(Json.parse("\"hello\"") == "hello", "parse simple string")
Test.assert(Json.parse("\"\"") == "", "parse empty string")

var escaped = Json.parse("\"line1\\nline2\"")
Test.assert(escaped == "line1\nline2", "parse newline escape")

var tabbed = Json.parse("\"col1\\tcol2\"")
Test.assert(tabbed == "col1\tcol2", "parse tab escape")

var quoted = Json.parse("\"say \\\"hi\\\"\"")
Test.assert(quoted == "say \"hi\"", "parse quote escape")

var slashed = Json.parse("\"a\\\\b\"")
Test.assert(slashed == "a\\b", "parse backslash escape")

var cr = Json.parse("\"a\\rb\"")
Test.assert(cr == "a\rb", "parse carriage return")

var bs = Json.parse("\"a\\bb\"")
Test.assert(bs == "a\bb", "parse backspace")

var ff = Json.parse("\"a\\fb\"")
Test.assert(ff == "a\fb", "parse form feed")

var unicodeA = Json.parse("\"\\u0041\"")
Test.assert(unicodeA == "A", "parse unicode escape A")

var unicodeE = Json.parse("\"\\u00e9\"")
Test.assert(unicodeE == "é", "parse unicode escape é")

var emptyArr = Json.parse("[]")
Test.assert(emptyArr is List, "parse empty array")
Test.assert(emptyArr.count == 0, "empty array count")

var numArr = Json.parse("[1,2,3]")
Test.assert(numArr is List, "parse number array")
Test.assert(numArr.count == 3, "number array count")
Test.assert(numArr[0] == 1, "array index 0")
Test.assert(numArr[1] == 2, "array index 1")
Test.assert(numArr[2] == 3, "array index 2")

var mixedArr = Json.parse("[1,\"two\",true,null]")
Test.assert(mixedArr.count == 4, "mixed array count")
Test.assert(mixedArr[0] == 1, "mixed arr 0 num")
Test.assert(mixedArr[1] == "two", "mixed arr 1 str")
Test.assert(mixedArr[2] == true, "mixed arr 2 bool")
Test.assert(mixedArr[3] == null, "mixed arr 3 null")

var nestedArr = Json.parse("[[1,2],[3,4]]")
Test.assert(nestedArr is List, "nested array outer")
Test.assert(nestedArr[0] is List, "nested array inner")
Test.assert(nestedArr[0].count == 2, "nested array inner count")
Test.assert(nestedArr[0][0] == 1, "nested array value")

var emptyObj = Json.parse("{}")
Test.assert(emptyObj is Map, "parse empty object")
Test.assert(emptyObj.count == 0, "empty object count")

var simpleObj = Json.parse("{\"a\":1}")
Test.assert(simpleObj is Map, "parse simple object")
Test.assert(simpleObj["a"] == 1, "object value")

var multiObj = Json.parse("{\"a\":1,\"b\":\"two\",\"c\":true,\"d\":null}")
Test.assert(multiObj.count == 4, "multi object count")
Test.assert(multiObj["a"] == 1, "multi obj a")
Test.assert(multiObj["b"] == "two", "multi obj b")
Test.assert(multiObj["c"] == true, "multi obj c")
Test.assert(multiObj["d"] == null, "multi obj d")

var nestedObj = Json.parse("{\"outer\":{\"inner\":42}}")
Test.assert(nestedObj["outer"] is Map, "nested object inner is map")
Test.assert(nestedObj["outer"]["inner"] == 42, "nested object value")

var arrInObj = Json.parse("{\"items\":[1,2,3]}")
Test.assert(arrInObj["items"] is List, "array in object")
Test.assert(arrInObj["items"].count == 3, "array in object count")

var wsInput = Json.parse("  \t\n\r  42  \t\n\r  ")
Test.assert(wsInput == 42, "parse with whitespace")

var slashInput = Json.parse("\"a\\/b\"")
Test.assert(slashInput == "a/b", "parse escaped slash")

var surrogatePair = Json.parse("\"\\uD83D\\uDE00\"")
Test.assert(surrogatePair == "😀", "parse surrogate pair emoji")

var roundtrip = Json.parse(Json.stringify({"key": "value"}))
Test.assert(roundtrip is Map, "stringify then parse roundtrip")
Test.assert(roundtrip["key"] == "value", "roundtrip value")

System.print("All JSON tests passed")
