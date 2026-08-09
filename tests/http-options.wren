// Outbound HTTP client option tests (bearer token, form bodies, cookie jar,
// query-string builder, timeouts, error message). These hit the local echo
// server (tests/echo), except the timeout case which targets the reserved,
// non-routable TEST-NET-1 address and must fail fast.
var target = Request.get("target")
var which = Request.get("which")

if (which == "token") {
  // options["token"] -> Authorization: Bearer <token>
  return Http.get(target + "/echo?mode=auth", {"token": "secret-token"})
}
if (which == "form") {
  // options["form"] -> x-www-form-urlencoded body
  return Http.post(target + "/echo?mode=form", {},
                   {"form": {"k": "v1", "v": "hello world"}})
}
if (which == "cookie") {
  // The Set-Cookie from the first call is sent back on the second
  var set = Http.get(target + "/echo?mode=setcookie")
  var jar = Http.get(target + "/echo?mode=cookie")
  return "%(set)|%(jar)"
}
if (which == "query") {
  // Http.url(base, params) appends and URL-encodes query parameters
  var built = Http.url(target + "/echo", {"mode": "echo", "msg": "hi there"})
  return Http.get(built)
}
if (which == "timeout") {
  // A tiny connect timeout fails fast and surfaces a curl error message
  var http = Http.new()
  http.method = "GET"
  var ok = http.call("http://192.0.2.1/", {"connectTimeout": 500})
  var msg = http.errorMessage != "" ? "error-present" : "error-empty"
  return "%(ok == false ? "false" : "true")|%(msg)"
}
return "no-case"
