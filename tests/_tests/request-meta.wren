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
