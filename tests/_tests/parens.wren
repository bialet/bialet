// Bialet dialect: `obj.name` falls back to calling a zero-argument `name()`
// when no getter `name` exists. `obj.name()` stays strict, and a getter still
// wins over a method of the same name.
class Greeter {
  construct new(name) { _name = name }
  greet() { "hello %(_name)" }
  need(extra) { "hello %(_name) %(extra)" }
  value { "getter" }
  value() { "method" }
  static total() { 42 }
}

class Loud is Greeter {
  construct new(name) { super(name) }
  shout() { super.greet }
}

var greeter = Greeter.new("ana")

// A zero-argument method runs without parentheses, on an instance, a static
// and a superclass method.
Test.assert(greeter.greet == "hello ana", "instance zero-arg method without parens")
Test.assert(Greeter.total == 42, "static zero-arg method without parens")
Test.assert(Loud.new("bob").shout == "hello bob", "super.greet finds greet()")
Test.assert(" x ".trim == "x", "core zero-arg method without parens")

// Parenthesized calls still work, and do not go through the fallback.
Test.assert(greeter.greet() == "hello ana", "parenthesized zero-arg method")
Test.assert(Greeter.total() == 42, "parenthesized static method")

// When a getter and a method share a name, the getter wins without parens and
// the method is reached with them.
Test.assert(greeter.value == "getter", "the getter wins without parens")
Test.assert(greeter.value() == "method", "parens call the method")

// A method that takes arguments is not a getter: without parens the call is
// still a method-not-found error, not a silent zero-argument call.
var needsArg = Fiber.new { greeter.need }
needsArg.try()
Test.assert(needsArg.error != null, "a method with arguments errors without parens")
Test.assert(needsArg.error.contains("does not implement 'need'"), "the arity error keeps the method name")

// A genuinely missing method still reports the original message.
var missing = Fiber.new { "x".nope }
missing.try()
Test.assert(missing.error == "String does not implement 'nope'.", "a missing getter keeps the old error")

// The fallback also reaches side-effecting methods, so `list.clear` clears.
var items = [1, 2, 3]
items.clear
Test.assert(items.count == 0, "side-effecting method without parens")
