class TestResponse {
  construct new(code, body, headers) {
    _code = code
    _body = body
    _headers = headers
  }
  code { _code }
  body { _body }
  headers { _headers }
  // The body with HTML tags removed and whitespace collapsed, so an assertion
  // cannot match an attribute or depend on how the template wraps a sentence.
  text {
    var out = []
    var inTag = false
    var lastSpace = false
    for (char in _body) {
      if (char == "<") {
        inTag = true
        continue
      }
      if (char == ">") {
        inTag = false
        continue
      }
      if (inTag) continue
      var isSpace = char == " " || char == "\n" || char == "\r" || char == "\t"
      if (isSpace) {
        if (!lastSpace && out.count > 0) out.add(" ")
        lastSpace = true
      } else {
        out.add(char)
        lastSpace = false
      }
    }
    return out.join("").trim()
  }
  // The hidden field rendered by Session.csrf, so a test can GET a form and
  // post its token back instead of hard-coding one.
  csrf() {
    var marker = 'name="_bialet_csrf" value="'
    var start = _body.indexOf(marker)
    if (start < 0) Fiber.abort("Response body does not contain a CSRF token")
    start = start + marker.count
    var end = _body.indexOf('"', start)
    if (end < 0) Fiber.abort("Malformed CSRF token in response body")
    return _body[start...end]
  }
}

class Test {
  construct new() {
    _path = ""
    _method = "GET"
    _postData = {}
    _headers = {}
    _response = null
    _csrf = false
    _csrfFrom = null
  }
  // A request runs in a fresh VM, so state a handler stores in a cookie (the
  // session identifier above all) survives only when the client sends it back.
  // Keep a jar shared by every Test call in this file: Test.get then Test.post
  // now sees the same session, the way a browser would.
  static cookies_ {
    if (!__cookies) __cookies = {}
    return __cookies
  }
  static cookie(name) { Test.cookies_[name] }
  static clearCookies() { __cookies = {} }
  // Set-Cookie is a response header like any other, but it can repeat and its
  // value is not a single token, so it gets parsed instead of stored verbatim.
  static storeCookies_(lines) {
    for (line in lines) {
      var parts = line.split(";")
      var pair = parts[0]
      var eq = pair.indexOf("=")
      if (eq <= 0) continue
      var name = pair[0...eq].trim()
      var value = pair[eq + 1..-1].trim()
      // An empty value or a non-positive Max-Age is a deletion; Cookie.delete
      // emits exactly that (empty value plus a 1970 expiry).
      var remove = value.count == 0
      for (i in 1...parts.count) {
        var attr = parts[i].trim().lower
        if (attr.startsWith("max-age=")) {
          var age = Num.fromString(attr["max-age=".count..-1])
          if (age != null && age <= 0) remove = true
        }
        if (attr.startsWith("expires=") && attr.contains("1970")) remove = true
      }
      if (remove) {
        Test.cookies_.remove(name)
      } else {
        Test.cookies_[name] = value
      }
    }
  }
  route(r) {
    _path = r
    return this
  }
  method(m) {
    _method = m
    return this
  }
  postData(d) {
    _postData = d
    return this
  }
  setHeader(n, v){
    _headers[n] = v
    return this
  }
  header(name, value) {
    if (!_response) run_()
    if (!_response.headers.containsKey(name)) {
      Fiber.abort("Expected response to contain header \"%(name)\"")
    }
    if (_response.headers[name] != value) {
      Fiber.abort("Expected header \"%(name)\" to be \"%(value)\" but was \"%(_response.headers[name])\"")
    }
    return this
  }
  header(name) {
    if (!_response) run_()
    if (!_response.headers.containsKey(name)) {
      Fiber.abort("Expected response to contain header \"%(name)\"")
    }
    return this
  }
  headerContains(name, value) {
    if (!_response) run_()
    if (!_response.headers.containsKey(name)) {
      Fiber.abort("Expected response to contain header \"%(name)\"")
    }
    if (!_response.headers[name].contains(value)) {
      Fiber.abort("Expected header \"%(name)\" to contain \"%(value)\" but was \"%(_response.headers[name])\"")
    }
    return this
  }
  status(code) {
    if (!_response) run_()
    if (code != _response.code) {
      Fiber.abort("Expected code to be %(code) but was %(_response.code)")
    }
    return this
  }
  contains(str) {
    if (!_response) run_()
    if (!_response.body.contains(str)) {
      Fiber.abort("Expected response body to contain \"%(str)\"")
    }
    return this
  }
  // Asserts on visible text (tags and attributes stripped) instead of raw
  // markup, so "device" cannot match width=device-width.
  textContains(str) {
    if (!_response) run_()
    if (!_response.text.contains(str)) {
      Fiber.abort("Expected visible text to contain \"%(str)\"")
    }
    return this
  }
  // The body with tags removed, for custom assertions.
  text() {
    if (!_response) run_()
    return _response.text
  }
  equals(expected) {
    if (!_response) run_()
    if (_response.body != expected) {
      Fiber.abort("Expected body to equal \"%(expected)\" but was \"%(_response.body)\"")
    }
    return this
  }
  notContains(str) {
    if (!_response) run_()
    if (_response.body.contains(str)) {
      Fiber.abort("Expected body not to contain \"%(str)\" but it did")
    }
    return this
  }
  json() {
    if (!_response) run_()
    return Json.parse(_response.body)
  }
  // Runs the request (if needed) and returns the CSRF token it rendered.
  csrf() {
    if (!_response) run_()
    return _response.csrf()
  }
  // Submits a protected form in one call: just before running, fetch the token
  // a GET rendered and add it to the form data.
  //
  //   Test.post("/login", {"user": "ana"}).withCsrf().status(302)
  //
  // With no argument the token comes from a GET of the same route, which is
  // where a form usually renders it. Pass a route, or an earlier Test /
  // TestResponse, when the form lives elsewhere or was already fetched.
  withCsrf() { withCsrf(null) }
  withCsrf(source) {
    _csrf = true
    _csrfFrom = source
    return this
  }
  runCsrf_() {
    if (_postData is String) {
      Fiber.abort("withCsrf() needs form data (a Map), not a raw body")
    }
    var token
    if (_csrfFrom == null || _csrfFrom is String) {
      // Carry the request headers over so an explicit Cookie header keeps the
      // token and the submission on the same session.
      var page = Test.new().route(_csrfFrom == null ? _path : _csrfFrom)
      for (name in _headers.keys) page.setHeader(name, _headers[name])
      token = page.csrf()
    } else {
      token = _csrfFrom.csrf()
    }
    var data = {}
    for (key in _postData.keys) data[key] = _postData[key]
    data["_bialet_csrf"] = token
    _postData = data
  }
  jsonContains(key) {
    if (!_response) run_()
    var data = Json.parse(_response.body)
    if (!(data is Map)) {
      Fiber.abort("Expected JSON object but body was: \"%(_response.body)\"")
    }
    if (!data.containsKey(key)) {
      Fiber.abort("Expected JSON to contain key \"%(key)\"")
    }
    return this
  }
  jsonEquals(key, value) {
    if (!_response) run_()
    var data = Json.parse(_response.body)
    if (!(data is Map)) {
      Fiber.abort("Expected JSON object but body was: \"%(_response.body)\"")
    }
    if (!data.containsKey(key)) {
      Fiber.abort("Expected JSON to contain key \"%(key)\"")
    }
    if (data[key] != value) {
      Fiber.abort("Expected JSON[\"%(key)\"] to be \"%(value)\" but was \"%(data[key])\"")
    }
    return this
  }
  jsonNotContains(key) {
    if (!_response) run_()
    var data = Json.parse(_response.body)
    if (!(data is Map)) {
      Fiber.abort("Expected JSON object but body was: \"%(_response.body)\"")
    }
    if (data.containsKey(key)) {
      Fiber.abort("Expected JSON not to contain key \"%(key)\" but it did")
    }
    return this
  }
  assert(condition, message) {
    if (!_response) run_()
    if (!condition) {
      Fiber.abort(message)
    }
    return this
  }
  apiHeader() { 
    setHeader("Content-Type", "application/json")
    return this
  }
  
  run_() {
    if (_csrf) runCsrf_()
    var body = ""
    if (_postData is String) {
      body = _postData
    } else if (_postData is Map && _postData.count > 0) {
      var parts = []
      for (key in _postData.keys) {
        var val = _postData[key]
        parts.add("%(key)=%(val)")
      }
      body = parts.join("&")
    }
    
    var message = "%(_method) %(_path) HTTP/1.1\r\n"
    message = message + "Host: localhost\r\n"
    if (!_headers.containsKey("Cookie") && Test.cookies_.count > 0) {
      var stored = []
      for (name in Test.cookies_.keys) {
        stored.add("%(name)=%(Test.cookies_[name])")
      }
      message = message + "Cookie: %(stored.join("; "))\r\n"
    }
    if (body.count > 0 && !_headers.containsKey("Content-Type")) {
      message = message + "Content-Type: application/x-www-form-urlencoded\r\n"
    }
    for (key in _headers.keys) {
      message = message + "%(key): %(_headers[key])\r\n"
    }
    if (body.count > 0) {
      message = message + "Content-Length: %(body.count)\r\n"
    }
    message = message + "\r\n"
    if (body.count > 0) {
      message = message + body
    }
    
    var result = runTestRequest_(_path, message)
    var headerMap = {}
    var setCookies = []
    if (result[2] != "") {
      var lines = result[2].split("\r\n")
      for (line in lines) {
        var colonIdx = line.indexOf(":")
        if (colonIdx > 0) {
          var name = line[0...colonIdx].trim()
          var value = line[colonIdx + 1..-1].trim()
          headerMap[name] = value
          if (name.lower == "set-cookie") setCookies.add(value)
        }
      }
    }
    Test.storeCookies_(setCookies)
    _response = TestResponse.new(result[0], result[1], headerMap)
  }
  
  static get(route) { Test.new().route(route).method("GET") }
  static post(route, params) { Test.new().route(route).method("POST").postData(params) }
  static apiGet(route) { Test.get(route).apiHeader() }
  static apiPost(route, params) { Test.post(route, Json.stringify(params)).apiHeader() }
  static apiPut(route, params) { Test.new().route(route).method("PUT").postData(Json.stringify(params)).apiHeader() }
  static apiDelete(route) { Test.new().route(route).method("DELETE").apiHeader() }

  // ---- Deterministic time and random (-T only) ----------------------------
  // freezeTime pins Date.now (and Cron, which reads Date.now) to the given
  // instant: a Num epoch, a Date, or "YYYY-MM-DD HH:MM:SS". SQLite's
  // datetime('now') is not affected, so a query that asks the database for the
  // time still moves.
  static freezeTime(when) {
    if (when == null) return Date.testNow_(null)
    if (when is Num) return Date.testNow_(when)
    if (when is Date) return Date.testNow_(when.unix)
    return Date.testNow_(Date.fromString("%(when)").unix)
  }
  static unfreezeTime { Date.testNow_(null) }
  // randomSeed(seed) makes Util.randomString deterministic; randomSeed() with
  // no argument restores the OS CSPRNG. Process-global, so a route handler's
  // fresh VM continues the same stream.
  static randomSeed() { Util.testRandom_(null) }
  static randomSeed(seed) { Util.testRandom_(seed) }

  // ---- Outbound HTTP stubs ------------------------------------------------
  // Register a canned response for calls to [pattern] (the full URL, '*' is a
  // glob). The registry lives in C so it also applies to the fresh VM every
  // Test.get request uses. First registered rule wins; unmatched calls still go
  // to the network. The registry is process-wide, so call clearHttpStubs() when
  // a file must not leak its stubs into the next one.
  //
  //   Test.stubHttp("https://api.example/*", {"status": 200,
  //     "headers": {"Content-Type": "application/json"}, "body": "{}"})
  static stubHttp(pattern, response) {
    if (!(response is Map)) Fiber.abort("Test.stubHttp: response must be a Map")
    var known = ["status", "headers", "body", "error", "errorMessage"]
    for (key in response.keys) {
      if (!known.contains(key)) {
        Fiber.abort("Test.stubHttp: unknown response key \"%(key)\"")
      }
    }
    var status = response.containsKey("status") ? response["status"] : 200
    var headers = response.containsKey("headers") ? response["headers"] : {}
    var body = response.containsKey("body") ? response["body"] : ""
    var error = response.containsKey("error") ? response["error"] : 0
    var errorMessage = response.containsKey("errorMessage") ? response["errorMessage"] : ""
    if (!(status is Num)) Fiber.abort("Test.stubHttp: status must be a number")
    if (!(headers is Map)) Fiber.abort("Test.stubHttp: headers must be a Map")
    if (!(error is Num)) Fiber.abort("Test.stubHttp: error must be a number")

    // setHeader_ validates the name (and rejects CRLF injection); headerValue
    // strips control characters from the value, like every other header.
    var normalized = {}
    for (name in headers.keys) {
      Util.setHeader_(normalized, name, Util.headerValue(headers[name]))
    }
    var lines = []
    for (name in normalized.keys) lines.add("%(name): %(normalized[name])")
    stubHttp_("%(pattern)", status, lines.join("\n"), body == null ? "" : "%(body)",
              error, errorMessage == null ? "" : "%(errorMessage)")
  }
  static clearHttpStubs() { clearHttpStubs_() }
  // Every matched call, in order: [{url, method, headers (lowercased), body}].
  // basicAuth is never recorded.
  static httpCalls {
    var calls = []
    for (row in httpCalls_()) {
      var headers = {}
      for (line in row[2].split("\n")) {
        var colon = line.indexOf(":")
        if (colon <= 0) continue
        Util.setHeader_(headers, line[0...colon].trim().lower, line[colon + 1..-1].trim())
      }
      calls.add({"url": row[0], "method": row[1], "headers": headers, "body": row[3]})
    }
    return calls
  }

  static assert(condition, message) {
    if (!condition) {
      Fiber.abort(message)
    }
  }
}

// Builds a Request from a raw HTTP message and runs module/component code
// against it without starting a server, then reads the Response it produced:
//
//   import "../_app/components" for Greeter
//   var f = Fixture.get("/notes/42").route("/notes").query("name", "ana").apply()
//   Response.out(Greeter.render())   // the code under test writes the response
//   Test.assert(f.status == 200, "status")
//   Test.assert(f.body == "hello ana", "body")
//   Test.assert(f.header("content-type").contains("text/html"), "header")
//
// Response.init always runs first, so one Fixture cannot inherit the previous
// one's status, headers, cookies or body. Request.init parses a form body only
// for POST, exactly as it does over HTTP; PUT/PATCH keep the raw body.
class Fixture {
  construct new(method, path) {
    _method = method
    _path = path
    _route = ""
    _query = {}
    _post = {}
    _rawBody = null
    _headers = {}
    _cookies = {}
    _files = ""
    _fields = ""
    _applied = false
    _headerMap = null
  }

  static get(path) { Fixture.new("GET", path) }
  static post(path, form) { Fixture.new("POST", path).form_(form) }
  static put(path, form) { Fixture.new("PUT", path).form_(form) }
  static patch(path, form) { Fixture.new("PATCH", path).form_(form) }
  static delete(path) { Fixture.new("DELETE", path) }
  static request(method, path) { Fixture.new("%(method)", path) }

  // Bulk form data, used by the static factories. The public one-field builder
  // is post(name, value).
  form_(form) {
    if (!(form is Map)) Fiber.abort("Fixture form data must be a Map")
    for (key in form.keys) _post[key] = form[key]
    return this
  }

  route(prefix) {
    _route = prefix
    return this
  }
  query(name, value) {
    _query[name] = value
    return this
  }
  post(name, value) {
    _post[name] = value
    return this
  }
  // A raw body overrides the form data built with post(name, value).
  body(raw) {
    _rawBody = raw
    return this
  }
  header(name, value) {
    Util.setHeader_(_headers, name, value)
    return this
  }
  cookie(name, value) {
    _cookies[Util.cookieToken(name, "Cookie name")] = Util.cookieValue(value, "Cookie value")
    return this
  }
  files(encoded) {
    _files = encoded
    return this
  }
  fields(encoded) {
    _fields = encoded
    return this
  }

  apply() {
    Response.init
    _headerMap = null

    var body = _rawBody
    if (body == null) {
      body = _post.count > 0 ? Util.params(_post) : ""
    } else {
      body = "%(body)"
    }
    var target = _path
    if (_query.count > 0) target = target + "?" + Util.params(_query)

    var message = "%(_method) %(target) HTTP/1.1\r\n"
    if (Util.getHeader_(_headers, "Host") == null) {
      message = message + "Host: localhost\r\n"
    }
    if (_cookies.count > 0 && Util.getHeader_(_headers, "Cookie") == null) {
      var stored = []
      for (name in _cookies.keys) stored.add("%(name)=%(_cookies[name])")
      message = message + "Cookie: %(stored.join("; "))\r\n"
    }
    if (body.count > 0 && Util.getHeader_(_headers, "Content-Type") == null) {
      message = message + "Content-Type: application/x-www-form-urlencoded\r\n"
    }
    for (name in _headers.keys) {
      message = message + "%(name): %(_headers[name])\r\n"
    }
    if (body.count > 0) message = message + "Content-Length: %(body.count)\r\n"
    message = message + "\r\n"
    if (body.count > 0) message = message + body

    Request.init(message, _route, _files, _fields)
    _applied = true
    return this
  }

  status {
    if (!_applied) apply()
    return Response.status
  }
  body {
    if (!_applied) apply()
    return Response.out
  }
  // Response header names lowercased for case-insensitive lookups; the last
  // Set-Cookie wins, like any other repeated header.
  headers {
    if (!_applied) apply()
    if (_headerMap == null) {
      var map = {}
      for (line in Response.headers.split("\r\n")) {
        var colon = line.indexOf(":")
        if (colon <= 0) continue
        map[line[0...colon].trim().lower] = line[colon + 1..-1].trim()
      }
      _headerMap = map
    }
    return _headerMap
  }
  header(name) {
    if (!_applied) apply()
    return headers["%(name)".trim().lower]
  }
}


// Marks the current test file as skipped instead of run. Call it as the
// file's only (or first) statement -- it does not abort the fiber, so any
// code after it still executes.
class Tests {
  static skip() {
    skip_()
  }
}
