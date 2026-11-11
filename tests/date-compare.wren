var d1 = Date.new(2024, 1, 1, 0, 0, 0)
var d2 = Date.new(2024, 6, 15, 12, 0, 0)
var d3 = Date.new(2024, 1, 1, 0, 0, 0)

var lt = d1 < d2
var gt = d2 > d1
var le = d1 <= d3
var ge = d2 >= d1
var eq = d1 == d3
var ne = d1 != d2

// Comparing to null (or to anything that is not a Date) is false/true, not a
// runtime error: an absent date is a normal value.
var eqNull = d1 == null
var neNull = d1 != null
var eqString = d1 == "2024-01-01"
var neString = d1 != "2024-01-01"

return lt.toString + "," + gt.toString + "," + le.toString + "," + ge.toString + "," +
  eq.toString + "," + ne.toString + "," + eqNull.toString + "," + neNull.toString + "," +
  eqString.toString + "," + neString.toString
