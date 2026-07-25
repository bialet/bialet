import "random" for Random
var r = Random.new()
var list = ["a", "b", "c", "d"]

var single = r.sample(list)
var validSingle = list.contains(single)

var sample2 = r.sample(list, 2)
var validCount = sample2.count == 2
var allInList = true
for (s in sample2) {
  if (!list.contains(s)) allInList = false
}

return validSingle.toString + "," + validCount.toString + "," + allInList.toString
