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
- **Markdown** - Render Markdown content to HTML
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

### init(message, route, files)

Initializes a new Request object with a given message, route, and files.

- `message`: The raw HTTP request message.
- `route`: The route the request is targeting.
- `files`: The uploaded files data.

### parseQuery(query)

Parses the query string of the request.

- `query`: The query string part of the URL.

### method

Returns the HTTP method of the request (uppercase, e.g., "GET", "POST", "PUT", "DELETE").

### uri

Returns the URI of the request.

### body

Returns the body content of the request.

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

### header(name)

Returns the value of a specified request header.

- `name`: The name of the header.

### get(name)

Returns the value of a specified query parameter.

- `name`: The name of the query parameter.

### query(name)

Alias for `get(name)`. Returns the value of a specified query parameter.

- `name`: The name of the query parameter.

### post(name)

> ⚠️ **CRITICAL: Returns `Null` when the key is not present.**
> The return type is `String | Null`. You **MUST** handle `null`
> before calling any string method (`.toString`, `.count`, `.trim`,
> concatenation, interpolation). This is the #1 production crash cause.
> **NEVER assume the field exists. ALWAYS provide a default or null check.**

Returns the value of a specified POST parameter, or `null` if not found.

- `name`: The name of the POST parameter.
- **Returns**: `String` if the key exists in the POST body, `Null` otherwise.

### route(pos)

Returns a specific part of the request route.

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

Sets a response header.

- `header`: The name of the header.
- `value`: The value of the header.

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

Fetches the file type based on its ID and output the file, setting the
appropriate headers.

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

Encodes a given input string to Base64.

- `input`: The string to encode.

### decodeBase64(input)

Decodes a Base64 encoded string.

- `input`: The Base64 encoded string to decode.

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
strings are sent as-is.

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

The jar is scoped per host and honors the `Domain`, `Path`, `Secure`, and
`Max-Age` cookie attributes, so a cookie set by one host is never sent to a
different host.

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
and methods for formatting, comparing, and manipulating dates relative to UTC.

### now

Static getter that returns a new `Date` instance representing the current date
and time in UTC. Equivalent to `Date.new()`.

```wren
var current = Date.now
System.log("Current time: %(current)")
```

### new()

Creates a new `Date` object representing the current date and time in UTC.

### new(date)

Creates a new `Date` object from a provided date string.

- `date`: The date string to initialize the `Date` object.

### new(date, tz)

Creates a new `Date` object from a provided date string and a specified timezone
offset.

- `date`: The date string to initialize the `Date` object.
- `tz`: The timezone offset to apply.

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

### tz=(tz)

Sets the timezone offset for the current `Date` object.

- `tz`: The timezone offset to apply.

### tz

Returns the timezone offset for the current `Date` object.

### format(format)

Formats the date according to the provided format string.

- `format`: The format string to apply.

### year

Returns the year component of the date.

### month

Returns the month component of the date.

### day

Returns the day component of the date.

### hour

Returns the hour component of the date.

### minute

Returns the minute component of the date.

### second

Returns the second component of the date.

### dayOfWeek

Returns the day of the week as a number (0 for Sunday, 6 for Saturday).

### dayOfYear

Returns the day of the year (1-366).

### date

Returns the date in `YYYY-MM-DD` format.

### time

Returns the time in `HH:MM:SS` format.

### unix

Returns the Unix timestamp of the date.

### iso

Returns the date as an ISO 8601 formatted string (`YYYY-MM-DDTHH:MM:SS`). Alias for `toString`.

### inUtc

Returns the date in UTC format.

### toString

Returns the date as an ISO 8601 string (`YYYY-MM-DDTHH:MM:SS`).

### +(plus)

Adds a date or time interval to the date.

- `plus`: The date or time interval to add.

### -(minus)

Subtracts a date or time interval from the date.

- `minus`: The date or time interval to subtract.

### diff(otherDate)

Returns the difference between the current date and `otherDate`.

- `otherDate`: The `Date` object to compare against.

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

Reads a `.md` file from disk and renders its content to HTML.

- `path`: The path to the Markdown file to render.

```wren
var content = Markdown.file("about.md")
// Reads about.md from the app directory and renders its Markdown to HTML
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
