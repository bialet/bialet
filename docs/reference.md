# Reference

This document provides the API reference for Bialet's built-in classes and external modules.

## Core Classes

The following classes are available by default in all Bialet applications without requiring imports:

- **Request** - Handle incoming HTTP requests
- **Response** - Construct and manage HTTP responses
- **Cookie** - Manage cookies
- **Session** - Manage session data
- **Json** - Handle JSON data
- **Util** - Utility helper methods
- **Config** - Manage configuration settings
- **Db** - Database interactions
- **Http** - Perform HTTP requests
- **Date** - Date and time operations
- **File** - File operations
- **Markdown** - Render Markdown content to HTML, list Markdown files and read front matter
- **System** - Logging and output utilities
- **HtmlNode** - A string of already-safe HTML that `{{ }}` leaves unescaped

## HtmlNode

`HtmlNode` wraps a string of already-rendered HTML. HTML string literals
(`<div>...</div>`), the result of a `{{ }}` block, and `String.raw` all produce
`HtmlNode` values. Interpolating an `HtmlNode` renders it verbatim, while
plain strings are auto-escaped.

### new(string)

Wraps a string as safe HTML. Only use this for markup you control or have
already escaped.

### toString

Returns the raw HTML string.

### +(other)

Concatenates the raw HTML with `other.toString`, returning a new `HtmlNode`.

### raw

Returns `this` — the node is already safe.

## External Classes

External classes must be imported explicitly using the GitHub shorthand or full URLs. See [External Modules](external-modules.md) for details on how to import, author, and manage external modules.

---

## Request

A class to handle incoming HTTP requests, parsing their content, and providing
convenient access to their components.

### init(message, route, files, fields)

Initializes a new Request object. Bialet calls this for you before your code
runs.

- `message`: The raw HTTP request message. For a `multipart/form-data` request
  it holds the request line and headers only (see [body](#body)).
- `route`: The route the request is targeting.
- `files`: The uploaded files data.
- `fields`: The text fields of a `multipart/form-data` body, already extracted.
  Empty for any other request.

### parseQuery(query)

Parses the query string of the request.

- `query`: The query string part of the URL.

### method

Returns the HTTP method of the request (uppercase, e.g., "GET", "POST", "PUT", "DELETE").

### uri

Returns the URI of the request.

### body

Returns the body content of the request.

For a `multipart/form-data` request the body is an empty string. The file
content never enters Wren, and the text fields are read with
[post(name)](#postname). Use `Request.post()` and `Request.file()` instead of
parsing the body.

### scheme

Returns `"https"` when the request reached the app over TLS, otherwise
`"http"`. Behind a proxy the decision comes from the first hop of
`X-Forwarded-Proto` or `Forwarded`, never from the transport itself.

### host

Returns the `Host` header, or `"localhost"` when it is absent. The header is
client-controlled: see the note under [baseUrl](#baseurl).

### baseUrl

Returns the absolute origin for links, with no trailing slash. It is
`Config.get("BIALET_BASE_URL")` when that row is set (trimmed, trailing slashes
removed); otherwise `scheme://host`.

```wren
Config.set("BIALET_BASE_URL", "https://app.example.com")
var canonical = Request.absoluteUrl("/articles/42")
```

Set `BIALET_BASE_URL` for canonical links, emails, feeds and cron jobs. Without
it a request behind a proxy can carry the wrong `Host`, and there is no request
at all in cron.

> ⚠️ Pitfall: `Host` is client-controlled, so `baseUrl` can return an
> attacker's domain if the proxy forwards an arbitrary value. Bialet never reads
> `X-Forwarded-Host`; use a fixed `BIALET_BASE_URL` when the origin matters.

### absoluteUrl(path)

Returns `path` made absolute against [baseUrl](#baseurl). A path starting with
`/` is appended as-is; one without a slash gets one. `null` or an empty path
returns `baseUrl` itself.

```wren
Request.absoluteUrl("/x")   // https://app.example.com/x
Request.absoluteUrl("x")    // https://app.example.com/x
```

### fullUrl

Returns [baseUrl](#baseurl) plus the request path and query string.

```wren
// On https://app.example.com/search?q=dates
Request.fullUrl    // https://app.example.com/search?q=dates
```

### isPost

Checks if the request method is POST.

### isJson

Checks if the request's content type is JSON.

### json()

Parses and returns the JSON body of the request. This is a convenience method equivalent to `Json.parse(Request.body)`.

Example:
```wren
if (Request.isJson) {
  var data = Request.json()
  var name = data["name"]
  var email = data["email"]
}
```

### header(name, default)

Returns the value of a specified request header, or `default` when the header is
absent.

- `name`: The name of the header.
- `default`: The value to return when the header is missing.

Only a missing header triggers the default. A header sent with an empty value
returns `""`.

### header(name)

Returns the value of a specified request header, or `null` when it is absent.

- `name`: The name of the header.

### get(name, default)

Returns the value of a specified query parameter, or `default` when the
parameter is absent. This is the recommended form when a fallback is wanted:

```wren
var page = Request.get("page", "1")
var search = Request.get("search", "")
```

- `name`: The name of the query parameter.
- `default`: The value to return when the parameter is missing.

Only `null` triggers the default. An empty value (`?q=`) returns `""` and a bare
flag (`?draft`) returns `true`. `Request.get(name) || "1"` still works for
strings, but it also replaces a `false` value; the default overload replaces
only `null`.

### get(name)

Returns the value of a specified query parameter, or `null` when it is absent.
Guard the result before a string operation, or use
[get(name, default)](#getname-default).

- `name`: The name of the query parameter.

### query(name, default)

Alias for `get(name, default)`.

### query(name)

Alias for `get(name)`.

### post(name, default)

Returns the value of a specified POST parameter, or `default` when the key is
absent. Use this instead of `Request.post(name) || ""` whenever a fallback is
wanted:

```wren
var name = Request.post("name", "")
```

- `name`: The name of the POST parameter.
- `default`: The value to return when the key is missing.

Only `null` triggers the default. A field submitted empty (`name=`) returns
`""`, so the fallback cannot hide a blank form submission. This is how
`isBlank` and a required-field check can tell "not sent" from "sent empty".

### post(name)

> ⚠️ **CRITICAL: Returns `Null` when the key is not present.**
> The return type is `String | Null`. You **MUST** handle `null`
> before calling any string method (`.toString`, `.count`, `.trim`,
> concatenation, interpolation). This is the #1 production crash cause.
> **Prefer `Request.post("name", "")` when a default is wanted; otherwise
> check `if (value != null)` before touching the value.**

Returns the value of a specified POST parameter, or `null` if not found.

- `name`: The name of the POST parameter.
- **Returns**: `String` if the key exists in the POST body, `Null` otherwise.

Works for `application/x-www-form-urlencoded` and `multipart/form-data` forms.
For a multipart form it returns the text fields only: a file part is read with
[file(name)](#filename) and never appears here. Line breaks in a value (a
`<textarea>`) are kept, with `\r\n` normalized to `\n`. If a field is repeated,
the last value wins.

### route(pos, default)

Returns a specific part of the request route, or `default` when the segment is
missing or empty.

- `pos`: The position of the route segment.
- `default`: The value to return when there is no segment at `pos`.

```wren
var id = Request.route(0, "index")
```

### route(pos)

Returns a specific part of the request route, or `null` when the segment is
missing or empty.

- `pos`: The position of the route segment.

### file(name)

Fetches a file from the database based on its name and returns it as a `File`
object if found. If the file is not found, returns `null`.

- `name`: The name of the file to retrieve.

### login(user, pass)

Performs basic HTTP authentication. Checks the provided credentials against the
request's Authorization header. If authentication fails, triggers `Response.login()`
to prompt for credentials.

- `user`: The expected username.
- `pass`: The expected password.
- Returns: `false` if authenticated successfully.

> ⚠️ Pitfall: on failure, `Request.login` sets the response status to `401`
> and the `WWW-Authenticate` header, but does **not** stop the script. Always
> `return` immediately when it's not `false`, or the rest of the handler
> still runs and its output becomes the body of the 401 response:
> ```wren
> if (Request.login("admin", "secret") != false) return
> // ... protected code ...
> ```

## Response

A class to construct and manage HTTP responses, including setting headers,
cookies, and the response body.

### init()

Initializes a new Response object.

### out

Returns the current output buffer, trimmed.

### status

Returns the current HTTP status code.

### headers

Returns the formatted response headers as a string.

### out(out)

Appends a string to the response body.

- `out`: The string to append.

### status(status)

Sets the HTTP status code for the response.

- `status`: The HTTP status code.

### addCookieHeader(value)

Adds a 'Set-Cookie' header to the response.

- `value`: The cookie value.

### header(header, value)

Sets a response header. Setting a header that already exists (in any letter
case) replaces it. The server writes the framing headers itself, so
`Content-Length`, `Transfer-Encoding`, `Connection`, `Keep-Alive`, `Upgrade`,
`TE`, `Trailer` and `Proxy-Connection` raise an error, as does a name with a
space, a colon or a control character.

- `header`: The name of the header.
- `value`: The value of the header. Control characters are removed.

### json(data)

Sends a JSON response.

- `data`: The data to be JSON-encoded.

### cors(origin, methods, headers)

Enables Cross-Origin Resource Sharing (CORS) by setting the appropriate headers.
When the request method is OPTIONS, it automatically responds with a 204 No Content
status and returns `true`. Otherwise, it returns `false`.

- `origin`: The allowed origin (e.g., `"*"` for all origins, or `"https://example.com"`).
- `methods`: Comma-separated list of allowed HTTP methods (e.g., `"GET, POST, PUT, DELETE, OPTIONS"`).
- `headers`: Comma-separated list of allowed headers (e.g., `"Content-Type, Authorization"`).

Example:
```wren
// Full control over CORS settings
if (Response.cors("https://example.com", "GET, POST", "Content-Type")) return

// Or use the default methods and headers for a specific origin
if (Response.cors("https://example.com")) return

// Or allow all origins with default methods and headers
if (Response.cors("*")) return

// Or use as a getter for the simplest case (allows all origins)
if (Response.cors) return
```

### cors(origin)

Convenience method that enables CORS with default methods and headers.

- `origin`: The allowed origin (e.g., `"*"` for all origins).

Default methods: `"GET, POST, PUT, DELETE, OPTIONS"`  
Default headers: `"Content-Type, Authorization"`

### cors()

Convenience method that enables CORS for all origins (`"*"`) with default methods and headers.

### cors

Property getter that enables CORS for all origins with default methods and headers.
This is the simplest way to enable CORS.

### file(id)

Sends a stored file as the response body, setting `Content-Type` from the row.
The type is treated as untrusted: script-free media types (PNG/JPEG/GIF/WebP/
AVIF, PDF, plain text) are served inline, everything else is sent as
`application/octet-stream` with `Content-Disposition: attachment`.

- `id`: The ID of the file to fetch.

### page(title, message)

Generates a simple HTML page response.

- `title`: The title of the page.
- `message`: The message to display on the page.

### end(code, title, message)

Ends the response with a specific status code and a simple HTML page.

- `code`: The HTTP status code.
- `title`: The title for the HTML page.
- `message`: The message for the HTML page.

### redirect(url)

Redirects the client to a specified URL.

- `url`: The target URL.

### forbidden()

Sends a 403 Forbidden response.

### notFound()

Sends a 404 Not Found response.

### login()

Generates a login page response.

## Cookie

A class for managing cookies, including parsing, setting, and deleting them.

### init()

Initializes a new Cookie object.

### parseHeader(headerValue)

Parses a 'Set-Cookie' header value.

- `headerValue`: The 'Set-Cookie' header value.

### set(name, value, options)

Sets a cookie with options.

- `name`: The name of the cookie.
- `value`: The value of the cookie.
- `options`: Additional options for the cookie (e.g., path, domain, secure).

### set(name, value)

Sets a cookie without options.

- `name`: The name of the cookie.
- `value`: The value of the cookie.

### delete(name)

Deletes a cookie.

- `name`: The name of the cookie to delete.

### get(name, default)

Returns the value of a specified cookie, or a default value if the cookie is not
found.

- `name`: The name of the cookie.
- `default`: The default value to return if the cookie is not found.

### get(name)

Returns the value of a specified cookie.

- `name`: The name of the cookie.

## Session

A class for managing session data, including setting and destroying sessions.

### new()

Creates a new Session instance.

### id

Returns the current session ID.

### name

Returns the name of the session cookie.

### name=(n)

Sets the name of the session cookie.

- `n`: The new name for the session cookie.

### get(key)

Retrieves a value from the session by its key.

- `key`: The session key to look up.

### set(key, value)

Sets a value in the session.

- `key`: The session key.
- `value`: The value to store.

### csrf

Returns the CSRF token for the current session as a hidden HTML input field.

### csrfOk

Validates the CSRF token from the current request against the session token.
Returns `true` if the token is valid.

### destroy()

Destroys the current session.

### rotate()

Mints a new session id, deletes the old id's rows, and sets the new cookie.
Call it after a successful login and **before** any authenticated write, so a
session identifier planted before authentication is worthless afterwards.
Returns the new id.

## Json

A class for handling JSON data, including parsing and stringifying.

### parse(string)

Parses a JSON string.

- `string`: The JSON string to parse.

### stringify(object)

Converts an object to a JSON string.

- `object`: The object to stringify.

## Util

A utility class providing various static helper methods.

### randomString(length)

Generates a random string of a specified length.

- `length`: The desired length of the random string.

### hash(password)

Generates a hash for the given password.

- `password`: The password to hash.

### verify(password, hash)

Verifies if a given password matches a hash.

- `password`: The password to verify.
- `hash`: The hash to compare against the password.

### encrypt(value, secret)

Encrypts `value` with AES-256-GCM and returns a text envelope you can store in a
`TEXT` column, a cookie, or a config row:

```wren
var sealed = Util.encrypt("honey", "password")
// bgcm1$600000$<salt>$<iv>$<ciphertext+tag>

Util.decrypt(sealed, "password")   // "honey"
```

- `value`: The string to encrypt. Its bytes are preserved exactly, including NUL
  bytes and non-ASCII.
- `secret`: Either a passphrase (stretched with PBKDF2-HMAC-SHA256, 600,000
  iterations, with a fresh random salt per message) or exactly **64 hex
  characters**, which are used directly as a 256-bit key.

Every call uses a fresh random salt and nonce, so encrypting the same value
twice produces different ciphertext. Encryption requires an OpenSSL 1.1+ build;
without one it raises a runtime error rather than using a weaker scheme.

### decrypt(value, secret)

Decrypts an envelope produced by `encrypt`. Returns the plaintext, or `null`
when the secret is wrong, the value was modified, or the value is not an
envelope at all. Those cases are deliberately indistinguishable.

- `value`: The envelope.
- `secret`: The same secret used to encrypt.

### toNum(val)

Converts a given value to a numeric type.

- `val`: The value to convert.

### hexToDec(hexStr)

Converts a hexadecimal string to its decimal equivalent.

- `hexStr`: The hexadecimal string to convert.

### toHex(byte)

Converts a byte to its hexadecimal string representation.

- `byte`: The byte to convert.

### urlDecode(str)

Decodes a URL-encoded string.

- `str`: The URL-encoded string to decode.

### lpad(s, count, with)

Left-pads a string with a specified character to a specified length.

- `s`: The string to pad.
- `count`: The desired length of the string after padding.
- `with`: The character to pad the string with.

### reverse(str)

Reverses a given string.

- `str`: The string to reverse.

### getPositionForIndex(text, index)

Finds the line and column position in a text for a specified index.

- `text`: The text to search.
- `index`: The index in the text.

### htmlEscape(str)

Escapes special HTML characters in a string to prevent XSS attacks.

- `str`: The string to escape.

### urlEncode(str)

Encodes a string for use in a URL by replacing certain characters with their
URL-safe equivalents.

- `str`: The string to encode.

### params(params)

Encodes a dictionary of parameters into a URL-encoded string.

- `params`: A dictionary of key-value pairs to encode.

### encodeBase64(input)

Encodes a given input string to Base64 using the standard alphabet with `=`
padding. Byte-oriented: it encodes the raw UTF-8 bytes, and round-trips with
`decodeBase64`.

- `input`: The string to encode.

### decodeBase64(input)

Decodes a Base64 encoded string. Returns `null` when the input is not valid
Base64, so a malformed credential produces a 401 rather than a runtime error.
The final group may omit its `=` padding.

- `input`: The Base64 encoded string to decode.

### base64UrlEncode(input)

Encodes a string to Base64url (RFC 4648 §5): the URL-safe `-`/`_` alphabet with
no `=` padding.

- `input`: The string to encode.

### sha256(input)

Returns the lowercase hex SHA-256 of the UTF-8 bytes of `input` (64 characters).
Non-string arguments are coerced to strings. Available in every build, with or
without OpenSSL.

- `input`: The string to hash.

### sha256Base64Url(input)

Returns `BASE64URL(SHA256(input))` with no padding. This is exactly PKCE's
`code_challenge` for the `S256` method (RFC 7636), useful for OAuth 2.1 and
OIDC flows.

- `input`: The string to hash and encode.

## Config

A class for managing configuration settings, with methods to get, set, and
delete configuration options.

### get(key)

Retrieves a configuration value by its key.

- `key`: The key of the configuration option.

### set(key, value)

Sets a configuration option to a given value.

- `key`: The key of the configuration option.
- `value`: The value to set.

### bool(key)

Retrieves a boolean configuration value.

- `key`: The key of the configuration option.

### num(key)

Retrieves a numeric configuration value.

- `key`: The key of the configuration option.

### enable(key)

Sets a configuration option to `"1"`. Convenience method for enabling a
feature flag — equivalent to `Config.set(key, "1")`.

- `key`: The key of the configuration option to enable.

```wren
Config.enable("beta_features")  // same as Config.set("beta_features", "1")
Config.bool("beta_features")    // true
```

### disable(key)

Sets a configuration option to `"0"`. Convenience method for disabling a
feature flag — equivalent to `Config.set(key, "0")`.

- `key`: The key of the configuration option to disable.

```wren
Config.disable("maintenance_mode")  // same as Config.set("maintenance_mode", "0")
Config.bool("maintenance_mode")     // false
```

### delete(key)

Deletes a configuration option.

- `key`: The key of the configuration option to delete.

### json(key)

Retrieves a configuration value as JSON.

- `key`: The key of the configuration option.

### json(key, val)

Sets a configuration option to a JSON value.

- `key`: The key of the configuration option.
- `val`: The JSON value to set.

### encrypt(key, value)

Encrypts `value` with AES-256-GCM under the deployment key from
`--secret-key-file` and stores the envelope as the value of `key`:

```wren
Config.encrypt("stripe_key", "sk_live_...")
Config.decrypt("stripe_key")            // "sk_live_..."
Config.get("stripe_key")                // "bgcm1$0$-..." — never the plaintext
```

The config key name is authenticated with the ciphertext, so copying an
encrypted row to a different key makes it fail to decrypt instead of silently
taking effect somewhere else.

### encrypt(key, value, secret)

Same, with an explicit passphrase or 64-hex-character key instead of the
deployment key.

### decrypt(key)

Returns the decrypted value of an encrypted row, or `null` when the row is
missing, the key does not match, or the value was never encrypted.

### decrypt(key, secret)

Same, with the explicit secret used to encrypt it.

## Secret

Read-only view of the deployment key passed with `--secret-key-file`. The key
material itself is never exposed to Wren: a value that Wren can see can end up
in a template, a log line, or a `%(...)` interpolation.

### available

`true` when the server was started with a secret key file. Check it at startup
if your app cannot run without one:

```wren
if (!Secret.available) Fiber.abort("Start bialet with --secret-key-file")
```

## Db

A class for database interactions, providing basic methods for migrations and
data manipulation.

### init()

Initializes the database connection.

### migrate(version, schema)

Performs database migrations to a specified version using the given schema.

- `version`: The target version of the database schema.
- `schema`: The schema to use for the migration.

### clean

Removes expired sessions and temporary files from the database.

### save(table, values)

Saves data to a specified table.

- `table`: The name of the table to save data to.
- `values`: The data to save, typically as a key-value pair object.

### delete(table, id)

Deletes data from a specified table based on its ID.

- `table`: The name of the table to delete data from.
- `id`: The ID of the data to delete.

Like `save`, the table name is interpolated into the statement, so it must be a
plain identifier (letters, digits and `_`, at most 64 characters) and it must
not name a framework table: a name that fails either check aborts with
`Invalid table name` or returns `false` instead of running. Never pass a
request-derived value here; map it to a literal name first.

```wren
Db.delete("users", 42)              // ok
Db.delete("users` WHERE 1=1; --", 1) // aborts: Invalid table name
Db.delete("BIALET_SESSION", 1)      // false
```

## Http

A class for making outbound HTTP requests, with shortcuts for the common
methods and an instance API for full control over the response.

For worked examples of every pattern (auth, headers, JSON, error handling),
see the [Making HTTP Calls](http-calls.md) guide.

### Options

Every method accepts an optional `options` map:

- `headers`: Map of header names to values, sent on the request.
- `basicAuth`: Map with `username` and `password` for HTTP Basic auth.
- `token`: String; sends `Authorization: Bearer <token>`.
- `form`: Map; sends the body as `application/x-www-form-urlencoded`.
- `timeout`: Number; total transfer timeout in milliseconds (default 20000).
- `connectTimeout`: Number; connect timeout in milliseconds (default 2000).

`Content-Type` defaults to `application/json` when not given in `headers` and
`form` is not used.

### Shortcut return values

The static shortcuts return:

- The parsed JSON value for 2xx responses with a JSON `Content-Type`.
- The body string for 2xx responses with any other `Content-Type`.
- `null` for non-2xx responses (e.g. 404, 500).
- `false` when the transport failed (DNS, connection, timeout).

### request(url, method, data, options)

Performs an HTTP request with the specified options.

- `url`: The URL to send the request to.
- `method`: The HTTP method (e.g., GET, POST, PATCH).
- `data`: The data to send with the request.
- `options`: Additional request options.

### get(url, options)

Performs a GET request to the specified URL with the given options.

- `url`: The URL to send the request to.
- `options`: Additional request options.

### post(url, data, options)

Performs a POST request to the specified URL with the given data and options.
`data` is sent as JSON unless a `Content-Type` header overrides it.

- `url`: The URL to send the request to.
- `data`: The data to send with the request.
- `options`: Additional request options.

### put(url, data, options)

Performs a PUT request to the specified URL with the given data and options.

- `url`: The URL to send the request to.
- `data`: The data to send with the request.
- `options`: Additional request options.

### delete(url, options)

Performs a DELETE request to the specified URL with the given options.

- `url`: The URL to send the request to.
- `options`: Additional request options.

### get(url)

Performs a simple GET request to the specified URL.

- `url`: The URL to send the request to.

### post(url, data)

Performs a simple POST request to the specified URL with the given data.

- `url`: The URL to send the request to.
- `data`: The data to send with the request.

### put(url, data)

Performs a simple PUT request to the specified URL with the given data.

- `url`: The URL to send the request to.
- `data`: The data to send with the request.

### delete(url)

Performs a simple DELETE request to the specified URL.

- `url`: The URL to send the request to.

### new()

Creates an `Http` instance for full control over the request and response.

```wren
var http = Http.new()
http.method = "POST"
http.postData = {"name": "Ada"}
var ok = http.call("https://api.example.com/users", {})
if (ok) {
  var status = http.status
  var body = http.body
  var type = http.headers("content-type")
}
```

### call(url, options)

Performs the request. Returns `true` when the transport succeeded — note this
includes non-2xx HTTP responses.

- `url`: The URL to send the request to.
- `options`: Additional request options.

### method

Set before `call` to choose the HTTP method. Defaults to `GET` when unset;
assigning `postData` defaults it to `POST`.

### postData

Set before `call` for the request body. Maps and lists are JSON-stringified;
strings are sent as-is. Any other value falls back to `toString` rather than
sending an empty body.

### status

Getter that returns the HTTP status code of the response.

### body

Getter that returns the raw response body as a string.

### headers

Getter that returns a map of all response headers, with lowercased keys and
values.

### headers(name)

Getter that returns a single response header by lowercased name, or `null` if
absent.

```wren
var type = http.headers("content-type")
```

### error

Getter that returns a non-zero value when the transport failed.

### errorMessage

Getter that returns a human-readable transport error message (the curl error
string), or `""` when the call succeeded.

### jar

Static getter that returns the current cookie jar as a `name=value; ...`
string, or `""` when empty. `Set-Cookie` response headers are stored in the
jar automatically and sent back on later calls unless a `Cookie` header is set
explicitly.

The jar is scoped per host: cookies are keyed on the host that set them, so a
cookie set by one host is never sent to a different host. `Path`, `Secure`, and
`Max-Age` are honored; the `Domain` attribute is ignored (host-only jar), since
a `Domain` covering a multi-label public suffix cannot be validated without an
embedded suffix list.

### query(params)

Static helper that returns a URL-encoded query string from a map of
parameters.

```wren
var qs = Http.query({"q": "hello world", "page": 2}) // "q=hello+world&page=2"
```

### url(base, params)

Static helper that appends URL-encoded query parameters to a URL, inserting
`?` or `&` as needed.

```wren
var url = Http.url("https://api.example.com/search", {"q": "hello"})
```

(date-reference)=

## Date

A class for managing date and time operations, supporting multiple constructors
and methods for formatting, comparing, and manipulating dates as wall clocks in
an explicit zone (UTC only when the zone says UTC).

### now

Static getter that returns a new `Date` instance representing the current date
and time in the application timezone (`BIALET_TIMEZONE`, or the process zone
when it is unset), the same value as `Date.new()` and with no UTC offset. Use
`.unix` when you need an unambiguous instant (see
[Stored timestamps](datetime.md#stored-timestamps)).

```wren
var current = Date.now
System.log("Current time: %(current)")
```

### new()

Creates a new `Date` object representing the current date and time in the
application timezone (`BIALET_TIMEZONE`, or the process zone when it is unset).

### new(date)

Creates a new `Date` object from a provided value.

- `date`: A date string (`"YYYY-MM-DD"` or `"YYYY-MM-DD HH:MM:SS"`), a number of
  **epoch seconds** (the same unit as `.unix`), or another `Date` (returned
  as-is).

```wren
Date.new("2024-09-13 10:30:00")
Date.new(1700000000)              // 2023-11-14 22:13:20 in Date.tz
Date.new(Date.now)
```

### new(date, tz)

Creates a new `Date` object from a provided value and a specified timezone.

- `date`: A date string, epoch seconds, or another `Date`.
- `tz`: The timezone offset or zone name to apply.

### new(year, month, day, hour, minute, second)

Creates a new `Date` object with the provided date and time components.

- `year`: The year component.
- `month`: The month component.
- `day`: The day component.
- `hour`: The hour component.
- `minute`: The minute component.
- `second`: The second component.

### new(year, month, day, hour, minute, second, tz)

Creates a new `Date` object with the provided date and time components and a
specified timezone offset.

- `year`: The year component.
- `month`: The month component.
- `day`: The day component.
- `hour`: The hour component.
- `minute`: The minute component.
- `second`: The second component.
- `tz`: The timezone offset to apply.

### new(year, month, day)

Creates a new `Date` object with the provided date, setting the time to
`00:00:00`.

- `year`: The year component.
- `month`: The month component.
- `day`: The day component.

### new(year, month, day, tz)

Creates a new `Date` object with the provided date and a specified timezone
offset, setting the time to `00:00:00`.

- `year`: The year component.
- `month`: The month component.
- `day`: The day component.
- `tz`: The timezone offset to apply.

### fromUnix(seconds)

Creates a `Date` from **epoch seconds**, formatted in `Date.tz`.

- `seconds`: Seconds since 1970-01-01 00:00:00 UTC.

```wren
var posted = Date.fromUnix(row["createdAt"].toNum)
```

### fromUnix(seconds, tz)

Creates a `Date` from epoch seconds, formatted in `tz`.

- `seconds`: Seconds since 1970-01-01 00:00:00 UTC.
- `tz`: The timezone offset or zone name to apply.

### tz=(tz)

Sets the timezone offset for the current `Date` object.

- `tz`: The timezone offset to apply.

### tz

Returns the timezone of the current `Date` object. It is the zone passed to the
constructor or set with `tz=`, otherwise the application default
(`BIALET_TIMEZONE`, or `""` for the process zone).

### format(format)

Formats the date according to the provided format string.

- `format`: The format string to apply.

### year

Returns the year component of the date.

### month

Returns the month component of the date.

### day

Returns the day component of the date.

### hours

Returns the hour component of the date.

### minutes

Returns the minute component of the date.

### seconds

Returns the second component of the date.

### dayOfWeek

Returns the day of the week as a number (0 for Sunday, 6 for Saturday).

### dayOfYear

Returns the day of the year (1-366).

### unix

Returns the Unix timestamp of the date.

### iso

Returns the date as an ISO 8601 formatted string (`YYYY-MM-DD HH:MM:SS`). Alias for `toString`.

### toString

Returns the date as an ISO 8601 string (`YYYY-MM-DD HH:MM:SS`).

### diff(otherDate)

Returns the difference in seconds between the current date and `otherDate`
(equivalent to `this.unix - otherDate.unix`).

- `otherDate`: The `Date` object to compare against.

### addDays(n)

Returns a new `Date` shifted by a whole number of **calendar days**, keeping the
hours, minutes, seconds and timezone. Negative `n` moves backwards. Leap days,
month ends, year boundaries and DST changes are handled by civil-calendar
arithmetic, so the wall clock does not move (`09:00` stays `09:00` even when the
day is 23 or 25 hours long).

- `n`: A whole number of days; a fractional number raises an error.

```wren
Date.new(2024, 2, 28).addDays(1)    // 2024-02-29 00:00:00
Date.new(2024, 1, 31).addDays(1)    // 2024-02-01 00:00:00
Date.new(2024, 3, 1).addDays(-1)    // 2024-02-29 00:00:00
```

### + (seconds)

Returns a new `Date` shifted forward by a number of **seconds** (the unit of
`.unix`). Adding another `Date` raises an error: adding two instants has no
meaning, so subtract them instead.

```wren
Date.new(2024, 1, 1) + 86400        // 2024-01-02 00:00:00
```

### - (seconds)

Returns a new `Date` shifted backwards by a number of **seconds**.

### - (otherDate)

Returns the number of **seconds** between the two dates, the same value as
`.diff(otherDate)`.

```wren
Date.new(2024, 1, 2) - Date.new(2024, 1, 1)   // 86400
```

### Comparison operators

`<`, `>`, `<=`, `>=`, `==`, `!=` all work between two `Date` instances,
comparing them by their underlying Unix timestamp (via `diff`).

To move an instant by a duration, use `+`/`-` with seconds; to move a calendar
date, use `addDays`. There is no built-in month arithmetic or `date` / `time` /
`inUtc` getter; clamp the day in a helper for months (see
[Recipes](recipes.md#dates-add-days-add-months-week-start-due-dates)).

## Markdown

A class for rendering Markdown content to HTML, providing static methods to
process both inline strings and Markdown files. See the
[Markdown guide](markdown.md) for the supported syntax and security notes.

### html(string)

Renders a Markdown string to HTML.

- `string`: The Markdown-formatted string to render.

```wren
var content = Markdown.html("## Hello **World**!")
// Renders: <h2>Hello <strong>World</strong>!</h2>
```

### file(path)

Reads a `.md` file from disk and renders its content to HTML. Returns `false`
when the file is missing, is not a `.md` or `.markdown` file, is outside the app
root, or has a path component starting with `_` or `.`.

- `path`: The path to the Markdown file to render, relative to the app root.

```wren
var content = Markdown.file("about.md")
// Reads about.md from the app directory and renders its Markdown to HTML
```

### list(dir)

Returns a sorted `List` of bare filenames in `dir`, relative to the app root.
Only `.md` and `.markdown` files (case-insensitive) are included. Names starting
with `_` or `.`, symlinks and non-regular files are skipped. A missing, empty or
invalid directory returns `[]`. The directory must stay inside the app root and
no path component may start with `_` or `.`.

- `dir`: Directory relative to the app root. `""` is the root.

```wren
var names = Markdown.list("cards")
// ["01.md", "02.md", ...]
```

### meta(path)

Reads leading YAML-ish front matter and returns a flat `Map` of `String` to
`String`. The block must start with a line containing only `---` and end with
another one. Keys and values are trimmed, one matching pair of single or double
quotes around a value is removed, and blank lines and `#` comments are ignored.
Returns `{}` when there is no front matter and `false` when the file is missing
or unreadable. Nested keys and lists are not supported; a later key overwrites
an earlier one. Applies the same `.md`-only, app-root and private-name rules as
`file(path)`.

- `path`: The Markdown file to read, relative to the app root.

```wren
var meta = Markdown.meta("cards/01.md")
meta["title"]
```

## Random

The `random` module is built in and must be imported:

```wren
import "random" for Random
```

### new()

Creates a generator seeded from the system.

### new(seed)

Creates a reproducible generator. `seed` is a `Num`, or a sequence of up to 16
numbers. The same seed and calls produce the same values.

### float()

Returns a number in `[0, 1)`.

### float(end)

Returns a number in `[0, end)`.

### float(start, end)

Returns a number in `[start, end)`.

### int()

Returns a non-negative whole number.

### int(end)

Returns a whole number in `[0, end)`.

### int(start, end)

Returns a whole number in `[start, end)`.

### sample(list)

Returns one random element. Aborts on an empty list.

### sample(list, count)

Returns `count` distinct random elements. Aborts when `count` is larger than the
list.

### shuffle(list)

Shuffles `list` in place. Returns `null`.

```wren
import "random" for Random

var rng = Random.new(42)
var dice = rng.int(1, 7)        // 1 to 6
var hand = rng.sample(deck, 5)
rng.shuffle(deck)
```

## File

### new(data)

Constructor that sets the file data.

- `data`: The data Map representing the file.

### get(id)

Fetches a file from the database by its ID if it is not temporary and sets the
file attributes.

- `id`: The ID of the file to retrieve.

### create(name, type, file, size)

Creates a new file in the database with the given name, type, content, and size.

- `name`: The name of the file.
- `type`: The MIME type of the file.
- `file`: The file content.
- `size`: The size of the file.

### create(name, type, file)

Creates a new file in the database with the given name, type, and content. The
size is determined by the content.

- `name`: The name of the file.
- `type`: The MIME type of the file.
- `file`: The file content.

### id

Returns the file's database ID.

### type

Returns the MIME type of the file.

### name

Returns the name of the file.

### size

Returns the size of the file in bytes.

### isTemp

Returns whether the file is temporary.

### createdAt

Returns the creation date of the file.

### destroy

Deletes the file from the database based on its ID.

### save

Marks the file as permanent by setting `isTemp` to false in the database.

### temp

Marks the file as temporary by setting `isTemp` to true in the database.

## Cron

### Cron.every(minutes, job)

Runs the job every `minutes`. It checks if the current minute is divisible by
the given value.

- `minutes` (Number): Interval in minutes.
- `job` (Fn): The callback function to execute.

```wren
Cron.every(10) { |d| System.log("Running every 10 minutes at %(d)") }
```

### Cron.at(hour, minute, job)

Runs the job at a specific hour and minute.

- `hour` (Number): Hour of the day (0–23).
- `minute` (Number): Minute of the hour (0–59).
- `job` (Fn): The callback function to execute.

```wren
Cron.at(9, 0) { |d| System.log("Running at 9:00 AM") }
```

### Cron.at(hour, minute, dayOfWeek, job)

Runs the job at a specific hour, minute, and day of the week.

- `hour` (Number): Hour of the day (0–23).
- `minute` (Number): Minute of the hour (0–59).
- `dayOfWeek` (Number): Day of the week (0=Sunday, 6=Saturday).
- `job` (Fn): The callback function to execute.

```wren
Cron.at(9, 0, 1) { |d| System.log("Running Monday at 9:00 AM") }
```

## System

A class providing static methods for printing output to the server's standard
output stream. Useful for logging, debugging, and tracing application behavior.

### print()

Prints an empty line.

```wren
System.print()
```

### print(obj)

Prints an object to stdout followed by a newline, then returns the object.

- `obj`: The object to print.

```wren
System.print("Hello World")
System.print(42)
```

### printAll(sequence)

Prints each element of a sequence on its own line.

- `sequence`: A list or other sequence to print.

```wren
System.printAll([1, 2, 3])
```

### write(obj)

Writes an object to stdout without a trailing newline, then returns the object.

- `obj`: The object to write.

```wren
System.write("Loading...")
```

### writeAll(sequence)

Writes each element of a sequence without newlines between them.

- `sequence`: A list or other sequence to write.

```wren
System.writeAll(["a", "b", "c"])
```

### log(obj)

Writes an object to stdout followed by a newline, then returns the object. Same
behavior as `print(obj)`. Supports Wren's string interpolation syntax `%(var)`
for formatted output.

- `obj`: The object to log.

```wren
System.log("Request from %(Request.uri) at %(Date.now)")
System.log("User %(name) logged in")
```

## Wren Core Extensions

Bialet extends several core Wren classes with additional methods to make development easier.

### Blankness (all objects)

`isBlank` and `isFilled` are getters available on every value, with Laravel-style
form semantics. They are exact inverses.

| Value | `isBlank` | `isFilled` |
|-------|-----------|------------|
| `null` | `true` | `false` |
| `""`, `"   "`, `"\t\n"` | `true` | `false` |
| `[]`, `{}`, empty range | `true` | `false` |
| `false`, `0` | `false` | `true` |
| any other value or object | `false` | `true` |

```wren
var name = Request.post("name", "")
if (name.isBlank) {
  errors.add("Name is required")
}
```

A sequence is blank when it has no elements. A whitespace-only string is blank
as well. `false` and `0` are values, not absences, so they are filled. Use
`isEmpty` when you mean exactly "zero elements": `"   ".isEmpty` is `false`
while `"   ".isBlank` is `true`.

### String Extensions

#### toBool

Converts a string to a boolean value by first converting to a number (using `toNum`), then checking if it's non-zero.

```wren
"1".toBool    // true
"0".toBool    // false
"42".toBool   // true
"".toBool     // false
```

#### safe

Escapes special HTML characters in a string to prevent XSS attacks. Replaces `&`, `<`, `>`, `"`, and `'` with their HTML entity equivalents.

```wren
"<script>alert('xss')</script>".safe  // "&lt;script&gt;alert(&apos;xss&apos;)&lt;/script&gt;"
```

`{{ }}` interpolation already calls this for plain values, so you do not need
`.safe` inside templates — adding it escapes the text twice. Use `.safe` when
building HTML from strings outside of interpolation, or to pre-escape a value
before storing it.

#### raw

Marks a string as already-safe HTML by wrapping it in an `HtmlNode`, so `{{ }}`
interpolation leaves it untouched.

```wren
var markup = "<b>bold</b>".raw
<p>{{ markup }}</p>  // <b>bold</b>, not escaped
```

#### lower

Converts the string to lowercase.

```wren
"Hello World".lower  // "hello world"
```

#### upper

Converts the string to uppercase.

```wren
"Hello World".upper  // "HELLO WORLD"
```

### List Extensions

#### first

Returns the first element of the list, or `null` if the list is empty.

```wren
[1, 2, 3].first  // 1
[].first          // null
```

### Sequence Extensions

The `Sequence` class is the base for all iterable collections (List, Map, Range, etc.).

#### to(Class)

Maps each element of a sequence to an instance of the specified class by calling `Class.new(element)` for each element.

```wren
class User {
  construct new(data) {
    _name = data["name"]
    _email = data["email"]
  }
  name { _name }
  email { _email }
}

// Convert list of maps to list of User instances
var usersData = [
  {"name": "Alice", "email": "alice@example.com"},
  {"name": "Bob", "email": "bob@example.com"}
]
var users = usersData.to(User)
// users is now a List of User objects

// Works with query results
var posts = `SELECT * FROM posts`.fetch.to(Post)
```

### Map Extensions

#### to(Class)

Converts a Map to an instance of the specified class by passing the map to the class constructor.

```wren
class Post {
  construct new(data) {
    _id = data["id"]
    _title = data["title"]
  }
  id { _id }
  title { _title }
}

var postData = {"id": 1, "title": "Hello World"}
var post = postData.to(Post)
// post is now a Post instance

// Works with single query results
var post = `SELECT * FROM posts WHERE id = ?`.first(1).to(Post)
```
