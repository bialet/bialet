// Null is forgiving in templates: a missing map key, count and map all have
// empty answers instead of crashing the page.
Test.assert(null["missing"] == null, "null[key] is null")
Test.assert(null.count == 0, "null.count is 0")
Test.assert(null.map { |x| x } is List, "null.map is a List")
Test.assert(null.map { |x| x }.count == 0, "null.map is empty")
Test.assert(null.to(String) == null, "null.to(Class) is null")
Test.assert(null.safe.toString == "", "null.safe renders empty")
Test.assert(null.toNum == 0, "null.toNum is 0")
Test.assert(null.toBool == false, "null.toBool is false")

// Map.get mirrors Config/Request: the bare form is this[key], the second form
// substitutes a default instead of returning null for a missing key.
var defaults = {"present": 7}
Test.assert(defaults.get("present") == 7, "map.get(key) returns the value")
Test.assert(defaults.get("missing") == null, "map.get(missing) is null")
Test.assert(defaults.get("missing", 0) == 0, "map.get(missing, default) returns the default")
