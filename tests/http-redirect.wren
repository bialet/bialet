// Redirect handling and cookie scoping. The transport used to follow redirects
// itself and merged every hop's headers into one buffer, so a cookie set by the
// redirect target was stored under - and sent to - the host of the original
// URL. Http.call now follows one hop at a time and scopes each hop's cookies to
// the host that emitted them.
var target = Request.get("target")
var other = Request.get("other")
var which = Request.get("which")
var otherHost = other == null ? "" : other.replace("http://", "").replace("https://", "")

if (which == "follow") {
  // A same-origin redirect is still followed and the final body returned.
  return Http.get(target + "/echo?mode=redirect-echo")
}

if (which == "cookie") {
  var followed = Http.get(target + "/echo?mode=redirect-set&to=" + otherHost)
  var sentToOrigin = Http.get(target + "/echo?mode=cookie")
  var targetJar = Http.jarFor(target + "/")
  var otherJar = Http.jarFor(other + "/")
  if (targetJar == "") targetJar = "no-cookie"
  if (otherJar == "") otherJar = "no-cookie"
  return "%(followed)|%(sentToOrigin)|%(targetJar)|%(otherJar)"
}

if (which == "userinfo") {
  // A URL whose userinfo text names another host: the jar key must be the host
  // the transport actually contacted, not the text before the first ':'.
  var userinfo = target.replace("http://", "")
  var fetched = Http.get("http://" + userinfo + "@" + otherHost + "/echo?mode=setcookie")
  var originJar = Http.jarFor(target + "/")
  var realJar = Http.jarFor(other + "/")
  if (originJar == "") originJar = "no-cookie"
  if (realJar == "") realJar = "no-cookie"
  return "%(fetched)|%(originJar)|%(realJar)"
}

if (which == "control") {
  // Without a redirect the cookie is emitted and replayed by its own host.
  var set = Http.get(target + "/echo?mode=setcookie")
  var jar = Http.get(target + "/echo?mode=cookie")
  return "%(set)|%(jar)"
}

return "unknown"
