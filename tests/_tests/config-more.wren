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
