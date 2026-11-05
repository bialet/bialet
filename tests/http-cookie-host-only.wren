// The outbound jar is host-only: a Set-Cookie Domain attribute can never widen
// a cookie to a suffix shared with a different registrant. Without this a
// response from evil.co.uk carrying "Domain=co.uk" was stored under '.co.uk'
// and jarFor() then sent it to victim.co.uk.
Http.storeCookies("Set-Cookie: a1=evil; Domain=co.uk", "http://evil.co.uk/")
var crossRegistrant = Http.jarFor("http://victim.co.uk/")
var own = Http.jarFor("http://evil.co.uk/")

Http.storeCookies("Set-Cookie: b1=evil; Domain=github.io", "http://evil.github.io/")
var hostedSuffix = Http.jarFor("http://victim.github.io/")
var parentOfOwn = Http.jarFor("http://github.io/")
var subOfOwn = Http.jarFor("http://api.evil.github.io/")

return "cross:[%(crossRegistrant)]|own:[%(own)]|hosted:[%(hostedSuffix)]|parent:[%(parentOfOwn)]|sub:[%(subOfOwn)]"
