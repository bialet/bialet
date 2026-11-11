// `toString` is a getter. The compiler accepts the parenthesized zero-argument
// form as the same call, so both spellings work for every receiver: core
// types, a class with its own toString getter, and a plain class.
class Custom {
  construct new() {}
  toString { "custom" }
}
class Plain { construct new() {} }

var s = "abc".toString()
var n = 42.toString()
var b = true.toString()
var z = null.toString()
var c = Custom.new().toString()
var p = Plain.new().toString()

return s + "|" + n + "|" + b + "|" + z + "|" + c + "|" + p.contains("Plain").toString
