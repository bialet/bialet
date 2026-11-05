// A Wren string is length-delimited and can hold a NUL, and the request parsers
// produce such strings (Util.urlDecode keeps %00, JSON keeps \u0000). The SQL
// bridge stores and binds C strings, so an embedded NUL used to make SQLite see
// a prefix of the value the application validated: "admin\0x" matched "admin".
// Query text and string parameters must now be refused instead.
var nul = String.fromByte(0)
var results = []

var paramFiber = Fiber.new {
  Query.fromString("SELECT ? AS v", ["admin" + nul + "x"]).first
}
paramFiber.try()
results.add("paramAborted:" + (paramFiber.error != null).toString)

var textFiber = Fiber.new {
  Query.fromString("SELECT 1 AS v WHERE 1 = 1 " + nul + " AND 0").first
}
textFiber.try()
results.add("textAborted:" + (textFiber.error != null).toString)

// A NUL-bearing value on a backtick query is refused too.
var valueFiber = Fiber.new {
  `SELECT ? AS v`.first(["a" + nul + "b"])
}
valueFiber.try()
results.add("bindAborted:" + (valueFiber.error != null).toString)

// A NUL-free string still binds, byte for byte.
var control = `SELECT ? AS v`.first(["admin"])
results.add("control:" + control["v"])

return results.join("|")
