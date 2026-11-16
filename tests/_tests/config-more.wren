Config.set("t_cfg_json", "{\"a\": 1}")
var parsed = Config.json("t_cfg_json")
Test.assert(parsed is Map, "Config.json getter returns Map")
Test.assert(parsed["a"] == 1, "Config.json getter value")

Config.json("t_cfg_obj", {"b": 2})
Test.assert(Config.json("t_cfg_obj") is Map, "Config.json setter stores JSON")
Test.assert(Config.json("t_cfg_obj")["b"] == 2, "Config.json roundtrip value")
Test.assert(Config.get("t_cfg_obj") == "{\"b\":2}", "Config stores JSON string")

Config.delete("t_cfg_json")
Test.assert(Config.get("t_cfg_json") == null, "Config.delete removes key")
Config.delete("t_cfg_obj")

// Documented truthiness: "1" and "true" (any case) are true, everything else
// is false, including a missing key.
Config.set("t_cfg_b", "1")
Test.assert(Config.bool("t_cfg_b") == true, "Config.bool 1")
Config.set("t_cfg_b", "TRUE")
Test.assert(Config.bool("t_cfg_b") == true, "Config.bool TRUE")
Config.set("t_cfg_b", "true")
Test.assert(Config.bool("t_cfg_b") == true, "Config.bool true")
Config.set("t_cfg_b", "yes")
Test.assert(Config.bool("t_cfg_b") == false, "Config.bool yes is false")
Config.set("t_cfg_b", "0")
Test.assert(Config.bool("t_cfg_b") == false, "Config.bool 0")
Test.assert(Config.bool("t_cfg_missing") == false, "Config.bool missing is false")

Config.enable("t_cfg_flag")
Test.assert(Config.bool("t_cfg_flag"), "Config.enable sets 1")
Config.disable("t_cfg_flag")
Test.assert(!Config.bool("t_cfg_flag"), "Config.disable sets 0")

Config.set("t_cfg_num", "42.5")
Test.assert(Config.num("t_cfg_num") == 42.5, "Config.num parses a number")
Test.assert(Config.num("t_cfg_missing") == null, "Config.num missing is null")

Config.delete("t_cfg_b")
Config.delete("t_cfg_flag")
Config.delete("t_cfg_num")
