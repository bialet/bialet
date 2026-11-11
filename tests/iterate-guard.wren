// A for-in over a value that is not an instance must raise a catchable Wren
// error, not dereference a tagged immediate: Object.iterate/iteratorValue are
// bound on the root class, so every value reached them, and a request-derived
// scalar (a JSON body of true, an absent Request.get parameter) killed the
// serving child with SIGSEGV.
var results = []
var sink = null
var fiber

fiber = Fiber.new {
  for (x in true) { sink = x }
}
fiber.try()
results.add("bool:" + (fiber.error != null).toString)

fiber = Fiber.new {
  for (x in 5) { sink = x }
}
fiber.try()
results.add("num:" + (fiber.error != null).toString)

fiber = Fiber.new {
  for (x in null) { sink = x }
}
fiber.try()
results.add("null:" + (fiber.error != null).toString)

fiber = Fiber.new {
  for (x in Request.get("missing")) { sink = x }
}
fiber.try()
results.add("missing:" + (fiber.error != null).toString)

fiber = Fiber.new {
  var row = null
  for (x in row.values) { sink = x }
}
fiber.try()
results.add("rowValues:" + (fiber.error != null).toString)

fiber = Fiber.new {
  var row = null
  for (x in row.keys) { sink = x }
}
fiber.try()
results.add("rowKeys:" + (fiber.error != null).toString)

fiber = Fiber.new {
  Query.fromString("SELECT 1", true)
}
fiber.try()
results.add("fromString:" + (fiber.error != null).toString)

fiber = Fiber.new {
  Query.fetchFromString("SELECT 1", 5)
}
fiber.try()
results.add("fetchFromString:" + (fiber.error != null).toString)

fiber = Fiber.new {
  Db.save("iterate_guard", true)
}
fiber.try()
results.add("saveBool:" + (fiber.error != null).toString)

fiber = Fiber.new {
  Db.save("iterate_guard", Request.get("missing"))
}
fiber.try()
results.add("saveMissing:" + (fiber.error != null).toString)

// The containers that really are iterable must keep working.
var total = 0
for (x in [1, 2, 3]) total = total + x
for (entry in {"a": 1}) total = total + 1
for (c in "ab") total = total + 1
results.add("control:" + total.toString)

return results.join("|")
