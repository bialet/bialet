var d1 = Date.new(2024, 1, 1, 0, 0, 0)
var d2 = Date.new(2024, 6, 15, 12, 0, 0)
var d3 = Date.new(2024, 1, 1, 0, 0, 0)

var lt = d1 < d2
var gt = d2 > d1
var le = d1 <= d3
var ge = d2 >= d1
var eq = d1 == d3
var ne = d1 != d2

return lt.toString + "," + gt.toString + "," + le.toString + "," + ge.toString + "," + eq.toString + "," + ne.toString
