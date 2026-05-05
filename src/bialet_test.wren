class TestResponse {
  construct new(code, body, headers) {
    _code = code
    _body = body
    _headers = headers
  }
  code { _code }
  body { _body }
  headers { _headers }
}

class Test {
  construct new() {
    _route = ""
    _method = "GET"
    _postData = {}
    _headers = {}
    _response = null
  }
  route(r) {
    _route = r
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
    
    var message = "%(_method) %(_route) HTTP/1.1\r\n"
    message = message + "Host: localhost\r\n"
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
    
    var result = runTestRequest_(_route, message)
    var headerMap = {}
    if (result[2] != "") {
      var lines = result[2].split("\r\n")
      for (line in lines) {
        var colonIdx = line.indexOf(":")
        if (colonIdx > 0) {
          var name = line[0...colonIdx].trim()
          var value = line[colonIdx + 1..-1].trim()
          headerMap[name] = value
        }
      }
    }
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
