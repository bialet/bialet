# Making HTTP Calls

Bialet can call other HTTP services from your `.wren` code. Use this to
integrate third-party APIs, pull remote data, or post webhooks. This page
covers the `Http` class end to end. For exposing your own API instead, see
[Building REST APIs](rest-api.md).

The `Http` class is a thin wrapper around libcurl (POSIX) or a raw sockets
client (Windows). It supports GET, POST, PUT, DELETE, and any other method,
custom headers, Basic and bearer-token auth, form or JSON bodies, per-call
timeouts, and a persistent cookie jar.

## Quick Start

```wren
// Simple GET - returns the response body
var ip = Http.get("https://api.ipify.org?format=json")
System.print(ip["ip"])

// Simple POST - data is sent as JSON
var created = Http.post("https://api.example.com/users", {"name": "Ada"})
System.print(created["id"])
```

The response of a successful (2xx) call is parsed as JSON when the server
responds with a JSON `Content-Type`, and returned as a plain string otherwise.
The exact rules are covered under [Response Handling](#response-handling).

## HTTP Methods

### GET

```wren
var users = Http.get("https://api.example.com/users")

// With options
var users = Http.get("https://api.example.com/users?active=1", {
  "headers": {"X-API-Key": "your-key"}
})
```

### POST

`Http.post` sends the body as JSON (`Content-Type: application/json`). Pass a
map, a list, or a pre-built JSON string. Use the `form` option for
`application/x-www-form-urlencoded` bodies instead (see
[Form-Encoded Bodies](#form-encoded-bodies)).

```wren
// Map is stringified automatically
Http.post("https://api.example.com/users", {"name": "Ada", "role": "admin"})

// Raw JSON string
Http.post("https://api.example.com/users", Json.stringify({"name": "Ada"}))

// Empty body is allowed
Http.post("https://api.example.com/hooks/trigger")
```

### PUT and DELETE

```wren
Http.put("https://api.example.com/users/1", {"name": "Grace"})

Http.delete("https://api.example.com/users/1")
```

### Other Methods (PATCH, HEAD, ...)

Use `Http.request(url, method, data, options)` for anything else:

```wren
var result = Http.request("https://api.example.com/users/1", "PATCH",
                          {"name": "Grace"}, {})
```

## Options

Every shortcut accepts an optional options map:

| Key               | Type       | Description                                              |
| ----------------- | ---------- | -------------------------------------------------------- |
| `headers`         | Map        | Header names to values, sent on the request              |
| `basicAuth`       | Map        | `username` / `password` for Basic auth                   |
| `token`           | String     | Sends `Authorization: Bearer <token>`                    |
| `form`            | Map        | Sends the body as `application/x-www-form-urlencoded`    |
| `timeout`         | Number     | Total transfer timeout in milliseconds (default 20000)   |
| `connectTimeout`  | Number     | Connect timeout in milliseconds (default 2000)           |

```wren
var options = {
  "headers": {"User-Agent": "bialet-app", "Accept": "application/json"},
  "basicAuth": {"username": "admin", "password": "secret"},
  "timeout": 10000,
  "connectTimeout": 3000
}
var data = Http.get("https://api.example.com/protected", options)
```

`Content-Type` defaults to `application/json` when you don't set one in
`headers` and don't use the `form` option. Every other common header goes
through `headers` directly.

## Authentication

### Bearer Token

Use the `token` option to send a `Authorization: Bearer <token>` header:

```wren
var zones = Http.get("https://api.cloudflare.com/client/v4/zones",
                     {"token": Config.get("API_TOKEN")})
```

This is equivalent to setting the header manually. Use the manual form when
you need a non-Bearer scheme or extra headers:

```wren
var options = {
  "headers": {
    "Authorization": "Bearer %(Config.get("API_TOKEN"))",
    "Content-Type": "application/json"
  }
}
var zones = Http.get("https://api.cloudflare.com/client/v4/zones", options)
```

### HTTP Basic Auth

```wren
var options = {
  "basicAuth": {"username": "admin", "password": "secret"}
}
var data = Http.get("https://api.example.com/basic-protected", options)
```

Bialet base64-encodes the credentials and sends the `Authorization: Basic ...`
header for you. Do **not** add a `basicAuth` entry and an `Authorization`
header at the same time — the header wins and the credentials are sent twice.

### Custom Auth Headers

Any auth scheme that fits in a header works with plain `headers`, including
cookies (`Cookie`), session tokens, and signatures:

```wren
var options = {
  "headers": {
    "Cookie": "session=abc123",
    "X-Signature": Util.sha256("payload")
  }
}
```

(form-encoded-bodies)=

## Form-Encoded Bodies

Pass a map to the `form` option to send
`application/x-www-form-urlencoded` data. Values are URL-encoded
automatically:

```wren
var options = {
  "form": {"username": "ada", "remember": "on"}
}
var data = Http.post("https://api.example.com/login", {}, options)
```

The `form` option overrides both the default JSON body and any `Content-Type`
header you set. This is the option to reach for when talking to traditional
web forms.

## Query Strings

Use `Http.url(base, params)` to append URL-encoded query parameters to a URL.
It inserts `?` or `&` as needed:

```wren
var url = Http.url("https://api.example.com/search",
                   {"q": "hello world", "page": 2})
// https://api.example.com/search?q=hello+world&page=2
```

`Http.query(params)` returns just the encoded `key=value&...` string if you
need to build the URL yourself.

## Cookies

Response `Set-Cookie` headers are collected into a cookie jar. On later calls,
the stored cookies are sent back as a `Cookie` header — useful for maintaining
a server-side session across calls:

```wren
// First call receives a Set-Cookie and stores it in the jar
Http.get("https://api.example.com/login", {"form": {"user": "ada"}})

// Subsequent calls automatically send Cookie: <stored cookies>
var profile = Http.get("https://api.example.com/me")
```

The jar is scoped per host. `Domain`, `Path`, `Secure`, and `Max-Age` cookie
attributes are honored, so a cookie set by one host is never sent to a
different host. If you want to opt out, set an explicit `Cookie` header in
`headers` — it takes precedence over the jar.

(response-handling)=

## Response Handling

### Shortcuts Return Convenience Values

The static shortcuts (`Http.get`, `Http.post`, ...) return:

- The parsed JSON value when the status is 2xx and `Content-Type` is JSON.
- The body string when the status is 2xx and `Content-Type` is not JSON.
- `null` when the status is not 2xx (e.g. 404, 500).
- `false` when the request itself failed (DNS, connection, timeout).

```wren
var result = Http.get("https://api.example.com/users")
if (result == false) {
  // Network error - DNS, connection refused, timeout, ...
} else if (result == null) {
  // Server replied with a non-2xx status
} else {
  // Success - JSON or string
}
```

### Full Control with `Http.new()`

When you need the status code, response headers, or raw body, build an `Http`
instance and call `call(url, options)` directly:

```wren
var http = Http.new()
http.method = "GET"
if (http.call("https://api.example.com/users", {})) {
  var code = http.status
  var body = http.body
  var contentType = http.headers("content-type")
  System.print("Status: %(code), type: %(contentType)")
} else {
  System.print("Call failed with error code %(http.error)")
}
```

`Http.new()` exposes:

| Member           | Description                                            |
| ---------------- | ------------------------------------------------------ |
| `call(url, opts)`| Performs the request. Returns `true` on transport success |
| `method`         | Set before `call`: GET (default), POST, PUT, ...       |
| `postData`       | Set before `call` for the request body                 |
| `status`         | HTTP status code of the response                       |
| `body`           | Raw response body (string)                             |
| `headers(name)`  | A single response header value, lowercased key         |
| `headers`        | Map of all response headers, lowercased                |
| `error`          | Non-zero when the transport failed                     |
| `errorMessage`   | Human-readable transport error message (curl string)   |

> **Pitfall:** `call` returns `true` for any HTTP response, including 404 and
> 500. Check `http.status` yourself when you need to distinguish them.

> **Pitfall:** response headers and their values are lowercased before they are
> stored. `http.headers("Content-Type")` will **not** find the key; use
> `http.headers("content-type")`.

### Response Headers

Response headers live in the `headers` map, keyed by lowercased name:

```wren
var rateLimit = http.headers("x-ratelimit-remaining")
```

## Example: Cloudflare API Client

This is a compact real-world client for the Cloudflare v4 API. It shows the
core patterns: a bearer token in headers, JSON bodies, and GET/POST/DELETE
shortcuts. (Adapted from a live deployment.)

```wren
class Cloudflare {
  static options {{
    "headers": {
      "Authorization": "Bearer %( Config.get("CLOUDFLARE_API_TOKEN") )",
      "Content-Type": "application/json"
    }
  }}
  static url(path) { "https://api.cloudflare.com/client/v4/%(path)" }
  static urlZoneRecords { url("zones/%( Config.get("CLOUDFLARE_ZONE_ID") )/dns_records") }

  static listRecords(domain) {
    Http.get(urlZoneRecords + "?name=%(domain["fqdn"])", options)
  }
  static createRecord(domain) {
    var data = Json.stringify({
      "type": "CNAME",
      "name": domain["fqdn"],
      "content": domain["dns"],
      "ttl": 1,
      "proxied": true
    })
    Http.post(urlZoneRecords, data, options)
  }
  static deleteRecord(record) { Http.delete("%(urlZoneRecords)/%(record)", options) }
}
```

Configuration values are read with `Config.get` instead of being hardcoded.
Store secrets in the [Config store](configuration.md), never in the `.wren`
files themselves.

## Error Handling

`Http.error` on a manually-built instance is a numeric code; `false` on the
shortcuts. `Http.errorMessage` carries the underlying error text (from curl)
for logging and debugging. The transport timeout defaults to 20 seconds total
with 2 seconds to connect — override per call with `timeout` and
`connectTimeout` (milliseconds), so a dead service returns an error instead of
hanging your app.

```wren
var http = Http.new()
http.method = "GET"
if (!http.call("https://api.example.com/health",
               {"timeout": 5000, "connectTimeout": 1000})) {
  // http.error is non-zero: DNS, connect, timeout, ...
  // http.errorMessage explains why, e.g. "Could not connect to server"
  return Response.json({"status": "down", "error": http.error,
                        "message": http.errorMessage})
}
```

## Pitfalls

- **The cookie jar does not support `Expires`.** `Max-Age`, `Domain`, `Path`,
  and `Secure` are honored, but a cookie that only carries an `Expires` date
  does not auto-expire in the jar.
- **`Request.post(name)` on the *other* side** returns `null` for missing keys
  — see [Building REST APIs](rest-api.md) when you build the receiving end.
- **Redirects are followed** automatically, up to 10 hops.
- **Response bodies are not size-capped** — a malicious endpoint could return
  an unbounded body. Only call APIs you trust.

## Missing Features

Not every HTTP client feature is implemented yet. Multipart/file uploads are
planned — see the
[Roadmap](https://github.com/bialet/bialet/blob/main/ROADMAP.md).
