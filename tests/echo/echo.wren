// Echo server used by the outbound HTTP (Http.*) tests. The default behavior
// returns the request method; the "mode" query parameter enables test helpers:
//   mode=header  -> returns the value of the X-Echo request header
//   mode=auth    -> returns the raw Authorization header (Basic auth)
//   mode=json    -> echoes the JSON body back as a JSON response
//   mode=status  -> sets the response status from the "code" parameter
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

if (Request.isPost) return "POST"
if (Request.method == "PUT") return "PUT"
if (Request.method == "DELETE") return "DELETE"
return "GET"
