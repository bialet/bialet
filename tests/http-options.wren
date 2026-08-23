// Outbound HTTP client option tests (bearer token, form bodies, cookie jar,
// query-string builder, timeouts, error message). These hit the local echo
// server (tests/echo), except the timeout case which targets the reserved,
// non-routable TEST-NET-1 address and must fail fast.
var target = Request.get("target")
var other = Request.get("other")
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
if (which == "cookie-scope") {
  // A cookie set on one host must not leak to a different host
  var set = Http.get(target + "/echo?mode=setcookie")
  var jar = Http.get(other + "/echo?mode=cookie")
  return "%(set)|%(jar)"
}
if (which == "cookie-path") {
  // A cookie scoped to /api must not be sent to /echo
  var set = Http.get(target + "/echo?mode=setcookie&path=/api")
  var jar = Http.get(target + "/echo?mode=cookie")
  return "%(set)|%(jar)"
}
if (which == "cookie-domain") {
  // A cookie for a foreign Domain must not be sent to the request host
  var set = Http.get(target + "/echo?mode=setcookie&domain=.example.com")
  var jar = Http.get(target + "/echo?mode=cookie")
  return "%(set)|%(jar)"
}
if (which == "cookie-secure") {
  // A Secure cookie must not be sent over plain http
  var set = Http.get(target + "/echo?mode=setcookie&secure=1")
  var jar = Http.get(target + "/echo?mode=cookie")
  return "%(set)|%(jar)"
}
if (which == "cookie-expire") {
  // Max-Age=0 expires the cookie immediately
  var set = Http.get(target + "/echo?mode=setcookie&maxage=0")
  var jar = Http.get(target + "/echo?mode=cookie")
  return "%(set)|%(jar)"
}
if (which == "cookie-override") {
  // Re-setting the same name replaces the stored value
  var set = Http.get(target + "/echo?mode=setcookie&name=session&value=first")
  var set2 = Http.get(target + "/echo?mode=setcookie&name=session&value=second")
  var jar = Http.get(target + "/echo?mode=cookie")
  return "%(set)|%(set2)|%(jar)"
}
if (which == "cookie-manual") {
  // An explicit Cookie header wins over the jar
  var set = Http.get(target + "/echo?mode=setcookie")
  var jar = Http.get(target + "/echo?mode=cookie", {"headers": {"Cookie": "manual=1"}})
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
  var ok = http.call("http://192.0.2.1/", {"connectTimeout": 200})
  var msg = http.errorMessage != "" ? "error-present" : "error-empty"
  return "%(ok == false ? "false" : "true")|%(msg)"
}
return "no-case"
