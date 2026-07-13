var jsonStr = '{"name":"Alice","age":30}'
var obj = Json.parse(jsonStr)
var backToJson = Json.stringify(obj)

return obj["name"] + "," + obj["age"].toString
