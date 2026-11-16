// Null is forgiving in templates: a missing map key, count and map all have
// empty answers instead of crashing the page.
Test.assert(null["missing"] == null, "null[key] is null")
Test.assert(null.count == 0, "null.count is 0")
Test.assert(null.map { |x| x } is List, "null.map is a List")
Test.assert(null.map { |x| x }.count == 0, "null.map is empty")
Test.assert(null.to(String) == null, "null.to(Class) is null")
Test.assert(null.safe.toString == "", "null.safe renders empty")
