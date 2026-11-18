// `obj.prop` reads the getter, so a zero-argument method with the same name
// makes the no-paren spelling ambiguous. The pair must not compile.
class Example {
  prop { 1 }
  prop() { 1 }
}
