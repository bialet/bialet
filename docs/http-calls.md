# Making HTTP Calls

Bialet can call other HTTP services from your `.wren` code. Use this to
integrate third-party APIs, pull remote data, or post webhooks. This page
covers the `Http` class end to end. For exposing your own API instead, see
[Building REST APIs](rest-api.md).

The `Http` class is a thin wrapper around libcurl (POSIX) or a raw sockets
client (Windows). It supports GET, POST, PUT, DELETE, and any other method,
plus custom headers and HTTP Basic authentication.

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
map, a list, or a pre-built JSON string.

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

Every shortcut accepts an optional options map with two keys:

| Key          | Type       | Description                                   |
| ------------ | ---------- | --------------------------------------------- |
| `headers`    | Map        | Header names to values, sent on the request   |
| `basicAuth`  | Map        | `username` / `password` for Basic auth        |

```wren
var options = {
  "headers": {"User-Agent": "bialet-app", "Accept": "application/json"},
  "basicAuth": {"username": "admin", "password": "secret"}
}
var data = Http.get("https://api.example.com/protected", options)
```

`Content-Type` defaults to `application/json` if you don't set one in
`headers`. Every other common header goes through `headers` directly.

## Authentication

### Bearer Token and API Keys

There is no dedicated helper. Send the token as a header — this is the
idiomatic pattern for most modern APIs:

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
shortcuts. The transport timeout is fixed at 20 seconds (2 seconds to connect),
so a dead service returns an error rather than hanging your app.

```wren
var http = Http.new()
http.method = "GET"
if (!http.call("https://api.example.com/health", {})) {
  // http.error is non-zero: DNS, connect, timeout, ...
  return Response.json({"status": "down", "error": http.error})
}
```

## Pitfalls

- **`Http.post` always sends JSON.** For `application/x-www-form-urlencoded`
  or `multipart/form-data`, set `Content-Type` and build the body string
  yourself.
- **`Request.post(name)` on the *other* side** returns `null` for missing keys
  — see [Building REST APIs](rest-api.md) when you build the receiving end.
- **Redirects are followed** automatically, up to 10 hops.
- **Response bodies are not size-capped** — a malicious endpoint could return
  an unbounded body. Only call APIs you trust.

## Missing Features

Not every HTTP client feature is implemented yet. Timeout configuration,
form-encoded bodies, file uploads, response cookies, and a bearer-token
shortcut are planned — see the
[Roadmap](https://github.com/bialet/bialet/blob/main/ROADMAP.md).
