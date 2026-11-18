// `get` and `set` are ordinary method names, not declaration keywords, so a
// getter `get`, a method `get(x)` and a method `set(v)` are all valid.
class Task {
  construct new() {}
  get { "value" }
  get(x) { "value %(x)" }
  set(v) { v }
}

var task = Task.new()
return "%(task.get)|%(task.get(2))|%(task.set(3))"
