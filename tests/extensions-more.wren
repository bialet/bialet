class User {
  construct new(data) {
    _name = data["name"]
  }
  name { _name }
}

var strToNum = "42".toNum
var strToNumBad = "abc".toNum

var viaMap = {"name": "Alice"}.to(User)
var viaList = [{"name": "A"}, {"name": "B"}].to(User).toList
var viaSequence = [1, 2, 3, 4, 5].take(2).join(",")

return "toNum:" + strToNum.toString + "," + (strToNumBad == null ? "null" : strToNumBad.toString) +
  "|map:" + viaMap.name +
  "|list:" + viaList.count.toString + "," + viaList[0].name + "," + viaList[1].name +
  "|take:" + viaSequence
