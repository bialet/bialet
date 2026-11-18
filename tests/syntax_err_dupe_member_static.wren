// Static members live in their own table, and the no-paren fallback works on
// the metaclass too, so a static getter and a static zero-argument method
// cannot share a name either.
class Example {
  static total { 1 }
  static total() { 1 }
}
