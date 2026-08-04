Config.set("test_key", "test_value")
Config.set("test_num", "42")
Config.set("test_bool", "1")
Config.disable("test_enable")
Config.enable("test_disable")

var str = Config.get("test_key")
var num = Config.num("test_num")
var bool = Config.bool("test_bool")
var disabled = Config.bool("test_enable")
var enabled = Config.bool("test_disable")

return str + "," + num.toString + "," + bool.toString + "," + disabled.toString + "," + enabled.toString
