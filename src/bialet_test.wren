class TestResponse {
  construct new(code, body, headers) {
    _code = code
    _body = body
    _headers = headers
  }
  code { _code }
  body { _body }
  headers { _headers }
  // The body with HTML tags removed, so an assertion cannot match an
  // attribute or a script body instead of the text a person sees.
  text {
    var out = []
    var inTag = false
    for (char in _body) {
      if (char == "<") {
        inTag = true
        continue
      }
      if (char == ">") {
        inTag = false
        continue
      }
      if (!inTag) out.add(char)
    }
    return out.join("")
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
  
  static assert(condition, message) {
    if (!condition) {
      Fiber.abort(message)
    }
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
