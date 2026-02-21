var list = [1, 2, 3]
var first = list.first

var empty = []
var emptyFirst = empty.first

Response.out(first.toString + "," + (emptyFirst == null ? "null" : emptyFirst.toString))
