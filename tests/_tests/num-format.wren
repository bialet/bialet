// Numbers mostly format through the integer fast path in wrenNumToString().
// Every expectation here is what snprintf("%.14g") produces, including the
// boundary where the fast path has to hand back to libc.
Test.assert((0).toString == "0", "zero")
Test.assert((-0.0).toString == "-0", "negative zero keeps its sign")
Test.assert((1).toString == "1", "one")
Test.assert((-1).toString == "-1", "minus one")
Test.assert((12345).toString == "12345", "a plain count")
Test.assert((10000000000000).toString == "10000000000000", "1e13 is integral")
Test.assert(
  (99999999999999).toString == "99999999999999",
  "largest integral value below 1e14"
)
Test.assert(
  (99999999999999.5).toString == "1e+14",
  "non-integral value near the bound falls back"
)
Test.assert((100000000000000).toString == "1e+14", "1e14 falls back")
Test.assert(
  (-99999999999999).toString == "-99999999999999",
  "negative bound is symmetric"
)
Test.assert((1.5).toString == "1.5", "a half")
Test.assert((0.1).toString == "0.1", "a tenth")
Test.assert((1.0 / 3.0).toString == "0.33333333333333", "a third")
Test.assert((0.00001).toString == "1e-05", "1e-5")
Test.assert((0.0001).toString == "0.0001", "1e-4")
Test.assert((0 / 0).toString == "nan", "nan")
Test.assert((1 / 0).toString == "infinity", "infinity")
Test.assert((-1 / 0).toString == "-infinity", "-infinity")
Test.assert("%(1234) rows" == "1234 rows", "interpolation uses the same path")
Test.assert((2..4).toString == "2..4", "a range prints both ends the same way")
