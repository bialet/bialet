// A getter and a method with parameters may share a name: the no-paren
// spelling still resolves to the getter, so there is no ambiguity.
class Example {
  construct new() {}
  prop { 1 }
  prop(x) { x }
  met() { "method only" }
}

System.print("%(Example.new.prop) %(Example.new.prop(2))")
