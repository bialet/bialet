if (Request.get("mode") == "header") {
  return Request.header("x-custom") ? Request.header("x-custom") : "none"
}

if (Request.isPost) {
  if (Request.isJson) {
    return "json|" + Request.uri + "|" + (Request.json()["key"] || "")
  }
  return "form|" + Request.uri + "|" + Request.body + "|" + Request.header("content-type")
}

return "get|" + Request.uri + "|" + (Request.query("foo") ? Request.query("foo") : "null")
