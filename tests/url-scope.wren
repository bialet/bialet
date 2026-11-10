// The redirect origin check must agree with the transport about who the peer
// is. Http.urlScope_ used to end the authority only at '/', so a fragment or a
// query before the path stayed inside it and the userinfo strip promoted the
// text after '@' to the origin: urlScope_("http://attacker.example#@api.partner.example")
// reported api.partner.example while libcurl dialed attacker.example, which let
// a redirect carry credentials across origins.
var weird = Http.urlScope_("http://attacker.example#@api.partner.example")
var plain = Http.urlScope_("http://127.0.0.1:18081/x")
var query = Http.urlScope_("http://a.example?@b.example")
var mixedCase = Http.urlScope_("HTTP://Example.COM:80/Path")

var results = []
results.add("weird:%(weird["host"]):%(weird["port"])")
results.add("plain:%(plain["host"]):%(plain["port"])")
results.add("query:%(query["host"]):%(query["port"])")
results.add("case:%(mixedCase["host"]):%(mixedCase["port"])")
results.add("crossPort:" + Http.sameOrigin_("http://127.0.0.1:18081/",
  "http://127.0.0.1:18082#@127.0.0.1:18081").toString)
results.add("sameHost:" + Http.sameOrigin_("http://127.0.0.1:18081/",
  "http://127.0.0.1:18081/other").toString)
results.add("httpsDefault:%(Http.urlScope_("https://a.example/")["port"])")

// A Location that carries credentials is refused instead of guessed about.
var fiber = Fiber.new {
  Http.resolveUrl_("http://127.0.0.1:18081/start", "http://user:pass@evil.example/next")
}
fiber.try()
results.add("userinfoAborted:" + (fiber.error != null).toString)
results.add("fragmentStripped:%(Http.resolveUrl_("http://a.example/dir/page", "/x#frag"))")

// The per-call transfer budget needs a monotonic millisecond clock: System.clock
// is CPU time and does not advance while a transfer waits on a socket.
var t1 = Util.monotonicMs
var t2 = Util.monotonicMs
results.add("monotonic:" + (t2 >= t1).toString)

return results.join("|")
