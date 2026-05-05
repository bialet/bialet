var first = Util.randomString(24)
var second = Util.randomString(24)

Test.assert(first.count == 24, "Expected Util.randomString to honor the requested length")
Test.assert(second.count == 24, "Expected Util.randomString to honor the requested length")
Test.assert(first != second, "Expected consecutive random strings to differ")
