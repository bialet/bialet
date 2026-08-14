// Echo server used by the outbound HTTP (Http.*) tests. The default behavior
// returns the request method; the "mode" query parameter enables test helpers:
//   mode=header    -> returns the value of the X-Echo request header
//   mode=auth      -> returns the raw Authorization header (Basic auth)
//   mode=json      -> echoes the JSON body back as a JSON response
//   mode=status    -> sets the response status from the "code" parameter
//   mode=setcookie -> sets a Set-Cookie header and returns "set"
//   mode=cookie    -> returns the Cookie header received
//   mode=form      -> echoes back the "k" and "v" form fields
//   mode=echo      -> returns the "msg" query parameter
var mode = Request.get("mode")

if (mode == "header") return Request.header("x-echo") || "no-header"
if (mode == "auth") return Request.header("authorization") || "no-auth"
if (mode == "json") {
  Response.json(Json.parse(Request.body))
  return
}
if (mode == "status") {
  var code = Request.get("code") || "200"
  Response.status(Num.fromString(code))
  return code
}
if (mode == "setcookie") {
  Response.header("Set-Cookie", "session=abc123; Path=/")
  return "set"
}
if (mode == "cookie") return Request.header("cookie") || "no-cookie"
if (mode == "form") return "%(Request.post("k") || "no-k")=%(Request.post("v") || "no-v")"
if (mode == "echo") return Request.get("msg") || "empty"

if (Request.isPost) return "POST"
if (Request.method == "PUT") return "PUT"
if (Request.method == "DELETE") return "DELETE"
return "GET"
