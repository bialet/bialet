// Same conflict in the other order: the zero-argument method is declared
// first, then the getter with the same name.
class Example {
  met() { "This is a method" }
  met { "This is a property" }
}
