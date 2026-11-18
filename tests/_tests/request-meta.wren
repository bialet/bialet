Test.get("/request-meta?foo=bar")
  .status(200)
  .equals("get|/request-meta|bar")

Test.get("/request-meta")
  .status(200)
  .equals("get|/request-meta|null")

Test.post("/request-meta", {"foo": "bar"})
  .status(200)
  .equals("form|/request-meta|foo=bar|application/x-www-form-urlencoded")

Test.apiPost("/request-meta", {"key": "jsonvalue"})
  .status(200)
  .equals("json|/request-meta|jsonvalue")

Test.get("/request-meta?mode=header")
  .setHeader("X-Custom", "hello-header")
  .status(200)
  .equals("hello-header")

Test.get("/request-meta?mode=header")
  .status(200)
  .equals("none")

// Defaults apply only to a missing value: an absent query parameter, route
// segment or header falls back, while a present one is returned as-is.
Test.get("/request-meta?mode=defaults&present=value&flag")
  .setHeader("X-Present", "header-value")
  .status(200)
  .equals("fallback|value|fallback|fallback|fallback|true|fallback|fallback|header-value")

// An empty POST field is a value, not a missing one, so the fallback must not
// replace it. A bare query flag stays true for the same reason.
Test.post("/request-meta?mode=defaults", {"empty": "", "present": "posted"})
  .status(200)
  .equals("fallback|fallback||posted|fallback|fallback|fallback|fallback|fallback")

// The public URL is built from the scheme and the Host header. The Test client
// sends Host: localhost, and fullUrl keeps the path and query.
Test.get("/request-meta?mode=url&x=1")
  .status(200)
  .equals("http|localhost|http://localhost|http://localhost/x|http://localhost/x|http://localhost/request-meta?mode=url&x=1|http://localhost|http://localhost")

// A proxy-terminated TLS request reports https through X-Forwarded-Proto, and
// every base follows it.
Test.get("/request-meta?mode=url")
  .setHeader("X-Forwarded-Proto", "https")
  .status(200)
  .equals("https|localhost|https://localhost|https://localhost/x|https://localhost/x|https://localhost/request-meta?mode=url|https://localhost|https://localhost")

// BIALET_BASE_URL wins over the request authority, is trimmed and loses its
// trailing slashes. Set it for the run, then restore whatever was there.
var previousBase = Config.get("BIALET_BASE_URL")
Config.set("BIALET_BASE_URL", "  https://example.test/  ")
Test.get("/request-meta?mode=urlbase")
  .status(200)
  .equals("https://example.test")
Test.get("/request-meta?mode=url")
  .setHeader("X-Forwarded-Proto", "https")
  .status(200)
  .equals("https|localhost|https://example.test|https://example.test/x|https://example.test/x|https://example.test/request-meta?mode=url|https://example.test|https://example.test")
if (previousBase == null) {
  Config.delete("BIALET_BASE_URL")
} else {
  Config.set("BIALET_BASE_URL", previousBase)
}
Test.get("/request-meta?mode=urlbase")
  .status(200)
  .equals("http://localhost")
