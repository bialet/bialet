Config.set("cfg_json", "{\"a\": 1}")
Config.json("cfg_obj", {"b": 2})
var fromJson = Config.json("cfg_json")
var fromObj = Config.json("cfg_obj")
var raw = Config.get("cfg_obj")
Config.delete("cfg_json")
var gone = Config.get("cfg_json")

return "a:" + fromJson["a"].toString +
  "|b:" + fromObj["b"].toString +
  "|raw:" + raw +
  "|gone:" + (gone == null).toString
