Config.set("test_key", "test_value")
Config.set("test_num", "42")
Config.set("test_bool", "1")

var str = Config.get("test_key")
var num = Config.num("test_num")
var bool = Config.bool("test_bool")

return str + "," + num.toString + "," + bool.toString
