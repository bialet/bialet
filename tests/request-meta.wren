if (Request.get("mode") == "header") {
  return Request.header("x-custom") ? Request.header("x-custom") : "none"
}

if (Request.get("mode") == "defaults") {
  // The two-argument forms default only on null, so an empty POST field and a
  // bare query flag must survive untouched.
  var values = [
    Request.get("missing", "fallback"),
    Request.get("present", "fallback"),
    Request.post("empty", "fallback"),
    Request.post("present", "fallback"),
    Request.post("missing", "fallback"),
    Request.query("flag", "fallback"),
    Request.route(9, "fallback"),
    Request.header("x-missing", "fallback"),
    Request.header("x-present", "fallback")
  ]
  return values.join("|")
}

if (Request.isPost) {
  if (Request.isJson) {
    return "json|" + Request.uri + "|" + (Request.json()["key"] || "")
  }
  return "form|" + Request.uri + "|" + Request.body + "|" + Request.header("content-type")
}

return "get|" + Request.uri + "|" + (Request.query("foo") ? Request.query("foo") : "null")
