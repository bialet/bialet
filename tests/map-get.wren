// Map.get mirrors Config.get / Request.get, with an optional default.
var m = {"a": 1}
var missing = m.get("missing")

return "a:" + m.get("a").toString +
  "|missing:" + (missing == null ? "null" : missing.toString) +
  "|default:" + m.get("missing", "none") +
  "|present:" + m.get("a", 99).toString +
  "|contains:" + m.containsKey("a").toString
