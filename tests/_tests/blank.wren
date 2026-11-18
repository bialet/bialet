// isBlank/isFilled are getters for form-oriented emptiness: null, a
// whitespace-only string and an empty sequence are blank. false and 0 are
// values, so they are filled. The pair is an exact inverse on every object.
class Widget {
  construct new() {}
}

var values = [
  null, "", "   ", "\t\n", "x", " x ", "0", "false",
  [], {}, 1...1, false, true, 0, 1, Widget.new()
]
var blank = [
  true, true, true, true, false, false, false, false,
  true, true, true, false, false, false, false, false
]

for (i in 0...values.count) {
  var value = values[i]
  Test.assert(value.isBlank == blank[i], "value %(i) has isBlank == %(blank[i])")
  Test.assert(value.isFilled == !blank[i], "value %(i) has isFilled == %(!blank[i])")
  Test.assert(value.isBlank != value.isFilled, "isBlank and isFilled are inverses for value %(i)")
}

// The two cases the Laravel semantics hinge on, spelled out.
Test.assert(false.isBlank == false, "false is a value, not blank")
Test.assert(false.isFilled == true, "false is filled")
Test.assert(0.isBlank == false, "0 is a value, not blank")
Test.assert(0.isFilled == true, "0 is filled")
Test.assert(null.isBlank == true, "null is blank")
Test.assert(null.isFilled == false, "null is not filled")
Test.assert("   ".isBlank == true, "whitespace is blank")
Test.assert("\t\n".isBlank == true, "tabs and newlines are blank")
Test.assert(" x ".isBlank == false, "a padded value is filled")

// isEmpty stays the zero-elements check: a string of spaces has three elements
// but is blank, and an empty list is both.
Test.assert("   ".isEmpty == false, "isEmpty counts characters, not blankness")
Test.assert("   ".isBlank == true, "the same string is blank")
Test.assert([].isEmpty == true, "an empty list is empty")
Test.assert([].isBlank == true, "an empty list is blank")
Test.assert([" "].isBlank == false, "a list holding a space is not blank")
