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

if (which == "case") {
  // Credential headers stop at the origin that issued them, whatever the
  // caller's spelling of the header name.
  var upper = Http.get(target + "/echo?mode=redirect-auth&to=" + otherHost,
    {"headers": {"AUTHORIZATION": "Bearer CASE"}})
  var mixed = Http.get(target + "/echo?mode=redirect-auth&to=" + otherHost,
    {"headers": {"AuthoriZation": "Bearer MIXED"}})
  var canonical = Http.get(target + "/echo?mode=redirect-auth&to=" + otherHost,
    {"headers": {"Authorization": "Bearer CANON"}})
  return "%(upper)|%(mixed)|%(canonical)"
}

if (which == "noauth") {
  // No basic auth configured means no Authorization header on the wire at all.
  return Http.get(target + "/echo?mode=auth")
}

if (which == "reuse") {
  // Per-call basicAuth and form body belong to the call that supplied them; a
  // reused instance must not carry either to another origin.
  var h = Http.new()
  var one = h.call(target + "/echo?mode=auth",
    {"basicAuth": {"username": "reuse", "password": "pw"}})
  var onOrigin = one ? h.body : "fail"
  var two = h.call(other + "/echo?mode=auth", {})
  var onOther = two ? h.body : "fail"
  h.method = "POST"
  var three = h.call(target + "/echo?mode=form", {"form": {"k": "a", "v": "b"}})
  var formHere = three ? h.body : "fail"
  var four = h.call(other + "/echo?mode=form", {})
  var formThere = four ? h.body : "fail"
  return "here:%(onOrigin)|other:%(onOther)|form:%(formHere)|formother:%(formThere)"
}

if (which == "shared") {
  // The framework may add Content-Type and Authorization to the request it
  // sends, but not to the caller's map: a shared map reused for another origin
  // must not carry the first call's bearer token.
  var shared = {"Accept": "application/json"}
  var first = Http.get(target + "/echo?mode=auth",
    {"headers": shared, "token": "bearer-shared"})
  var second = Http.get(other + "/echo?mode=auth", {"headers": shared})
  var kept = shared.containsKey("Authorization") ? "mutated" : "intact"
  return "%(first)|%(second)|%(kept)"
}

if (which == "control") {
  // Without a redirect the cookie is emitted and replayed by its own host.
  var set = Http.get(target + "/echo?mode=setcookie")
  var jar = Http.get(target + "/echo?mode=cookie")
  return "%(set)|%(jar)"
}

return "unknown"
