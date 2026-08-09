// Outbound HTTP client tests. These hit the local echo server (tests/echo),
// so they do not depend on external services.
var target = Request.get("target")
var out = ""

// Custom request header round-trip
var header = Http.get(target + "/echo?mode=header", {"headers": {"X-Echo": "hello"}})
out = out + header + "|"

// HTTP Basic auth round-trip (curl sends "Basic <base64 of admin:secret>")
var auth = Http.get(target + "/echo?mode=auth",
                    {"basicAuth": {"username": "admin", "password": "secret"}})
out = out + auth + "|"

// POST sends a JSON body and parses the JSON response
var json = Http.post(target + "/echo?mode=json", {"k": "v"})
out = out + json["k"] + "|"

// Non-2xx responses return null from the shortcuts
var missing = Http.get(target + "/echo?mode=status&code=404")
out = out + (missing == null ? "null" : "not-null")

return out
