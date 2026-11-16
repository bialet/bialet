// Responses with a status the app picks explicitly. Named cases keep the
// original bodies; any other 3-digit code goes straight through so the reason
// phrase table in get_http_status_description() can be exercised for codes no
// app path in this suite would otherwise emit.
var code = Request.get("code")
if (code == "404") {
  Response.status(404)
  return "not found"
} else if (code == "201") {
  Response.status(201)
  return "created"
} else if (code == "500") {
  Response.status(500)
  return "error"
} else {
  var numeric = Num.fromString(code == null ? "" : code)
  if (numeric != null && numeric >= 100 && numeric <= 599) {
    Response.status(numeric)
    return "status-%(numeric)"
  }
  Response.status(200)
  return "ok"
}
