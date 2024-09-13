//
//  This file is part of Bialet, which is licensed under the
//  GNU General Public License, version 2 (GPL-2.0).
//
//  Copyright (c) 2023 Rodrigo Arce
//
//  SPDX-License-Identifier: GPL-2.0-only
//
//  For full license text, see LICENSE.md.
//

class Request {
  static init(message, route) {
    __message = message
    __headers = {}
    __get = {}
    __post = {}
    var lines = message.split("\n")
    var tmp = lines.removeAt(0).split(" ")
    __method = tmp[0]
    __fullUri = tmp[1]
    __body = ""
    __route = __fullUri.trimStart(route.trimEnd("#")).split("/")
    var uriSeparator = __fullUri.indexOf("?")
    if (uriSeparator > 0) {
      __uri = __fullUri[0...uriSeparator]
      __get = parseQuery(__fullUri[uriSeparator+1...__fullUri.count])
    }
    var startBody = false
    var headerName
    var headerValue
    for (line in lines) {
      if (line.trim() == "") {
        startBody = true
        continue
      }
      if (!startBody) {
        tmp = line.split(":")
        headerName = tmp.removeAt(0).trim().lower
        headerValue = tmp.join(":").trim()
        if (headerName == "cookie") {
          Cookie.parseHeader(headerValue)
        }
        __headers[headerName] = headerValue
      } else {
        __body = __body + line
      }
    }
    if (__method.upper == "POST") {
      __post = parseQuery(__body)
    }
  }

  static parseQuery(query) {
    var all = {}
    query = query.trim()
    if (query == "") {
      return all
    }
    // @TODO Support arrays in URL and POST query
    query.split("&").each{|q|
      var value = true
      var tmp = q.split("=")
      var key = Util.urlDecode(tmp[0])
      if (tmp.count > 1) {
        value = Util.urlDecode(tmp[1])
      }
      all[key] = value
    }
    return all
  }

  // Getters
  static method { __method }
  static uri { __uri }
  static body { __body }
  static isPost { __method == "POST" }
  static isJson { header("content-type") == "application/json" }
  static header(name) { __headers[name] ? __headers[name]:null }
  static get(name) { __get[name] ? __get[name]:null }
  static post(name) { __post[name] ? __post[name]:null }
  static route(pos) { __route.count > pos && __route[pos] != "" ? __route[pos]:null}

}

class Response {
  static init() {
    __headers = {"Content-Type": "text/html; charset=UTF-8"}
    __cookies = []
    __status = 200
    __out = ""
  }

  // Getters
  static out { __out.trim() }
  static status { __status }
  static headers { __cookies.join("\r\n") +  __headers.keys.map{|k| k + ": " + __headers[k] + "\r\n"}.join() }

  static out(out) { __out = __out + "\r\n" + out }
  static status(status) { __status = status }
  static addCookieHeader(value) { __cookies.add("Set-Cookie: %(value)") }
  static header(header, value) { __headers[header.trim()] = value.trim() }

  static json(data) {
    header("Content-Type", "application/json; charset=UTF-8")
    out(Json.stringify(data))
  }

  static page(title, message) { '<!DOCTYPE html><body style="font:2.3rem system-ui;text-align:center;margin:2em;color:#024"><h1>%( title )</h1><p>%( message )</p><p style="font-size:.8em;margin-top:2em">Powered by 🚲 <b><a href="https://bialet.dev" style="color:#007FAD" >Bialet' }
  static end(code, title, message) { status(code) && out(page(title, message)) }

  static redirect(url) {
    header("Location", url)
    return end(302, "➡️ Redirect to", '<a href="%(url)">%(url)</a>')
  }
  static forbidden() { end(403, "🚫 Forbidden", "Sorry, you don't have permission to access this page") }
  static login() {
    header("WWW-Authenticate", 'Basic realm="Login required"')
    return end(401, "🔒 Needs login", '<a href="javascript:location.reload()">Sign in</a> to access this page')
  }
}

class Cookie {
  static init() { __cookies = {} }
  static parseHeader(headerValue) {
    __cookies = {}
    for (cookieStr in headerValue.split(";")) {
      var cookie = cookieStr.split("=")
      if (cookie.count > 1) {
        __cookies[cookie[0].trim()] = cookie[1].trim()
      }
    }
  }
  static set(name, value, options) {
    Response.addCookieHeader("%(name)=%(value); %( options.keys.map{|k| "%(k)=%(options[k])"}.join("; ") )")
    __cookies[name] = value
  }
  static set(name, value){ set(name, value, {}) }
  static delete(name){ set(name, "", {"expires": "Thu, 01 Jan 1970 00:00:00 GMT"}) }
  static get(name, default){ __cookies != null && __cookies[name] ? __cookies[name]: default }
  static get(name){ get(name, null) }
}

class Session {
  static name { __name ? __name : "BIALETSESSID" }
  static name=(n) { __name = n }
  static destroy() {
    var id = Cookie.get(Session.name)
    Cookie.delete(Session.name)
    `DELETE FROM BIALET_SESSION WHERE id = ? OR updatedAt < date('now', '-1 month')`.query([id])
  }
  construct new() {
    _id = Cookie.get(Session.name)
    if (!_id) {
      _id = Util.randomString(40)
      Cookie.set(Session.name, _id)
    }
    __values = {}
    var res = `SELECT key, val FROM BIALET_SESSION WHERE id = ?`.fetch([_id])
    if (res && res.count > 0) {
      res.each{|r| __values[r["key"]] = r["val"] }
    }
  }
  id { _id }
  get(key) { __values[key] ? __values[key] : null }
  set(key, value) {
    __values[key] = value
    `REPLACE INTO BIALET_SESSION (id, key, val, updatedAt) VALUES (?, ?, ?, CURRENT_TIMESTAMP)`.query(_id, key, "%(value)")
  }
  csrf {
    var token = Util.randomString(60)
    set("_bialet_csrf", token)
    return '<input type="hidden" name="_bialet_csrf" value="%( token )">'
  }
  csrfOk { get("_bialet_csrf") == Request.post("_bialet_csrf") }
}

// Json library and Util functions are from Matthew Brandly
// https://github.com/brandly/wren-json
class Json {
  static parse(string) {
    return JsonParser.new(string).parse
  }

  static stringify(object) {
    return JsonStringifier.new(object).toString
  }

  static tokenize(string) {
    return JsonScanner.new(string).tokenize
  }
}

class JsonStringifier {
  construct new(object) {
    _object = object
  }

  toString { stringify(_object) }

  stringify(obj) {
    if (obj is Null) {
      return "null"
    }
    if (obj is Num || obj is Bool) {
      return obj.toString
    } else if (obj is String) {
      var substrings = []
      // Escape special characters
      for (char in obj) {
        if (char == "\"") {
          substrings.add("\\\"")
        } else if (char == "\\") {
          substrings.add("\\\\")
        } else if (char == "\b") {
          substrings.add("\\b")
        } else if (char == "\f") {
          substrings.add("\\f")
        } else if (char == "\n") {
          substrings.add("\\n")
        } else if (char == "\r") {
          substrings.add("\\r")
        } else if (char == "\t") {
          substrings.add("\\t")
        } else if (char.bytes[0] <= 0x1f) {
          // Control characters!
          var byte = char.bytes[0]
          var hex = Util.lpad(Util.toHex(byte), 4, "0")
          substrings.add("\\u" + hex)
        } else {
          substrings.add(char)
        }
      }
      return "\"" + substrings.join("") + "\""

    } else if (obj is List) {
      var substrings = obj.map { |o| stringify(o) }
      return "[" + substrings.join(",") + "]"

    } else if (obj is Map) {
      var substrings = obj.keys.map { |key|
        return stringify(key) + ":" + stringify(obj[key])
      }
      return "{" + substrings.join(",") + "}"
    }
  }
}

class JsonParser {
  construct new(input) {
    _input = input
    _tokens = []
  }

  valueTypes { [JsonToken.String, JsonToken.Number, JsonToken.Bool, JsonToken.Null] }

  parse { nest(JsonScanner.new(_input).tokenize) }

  nest(tokens) {
    if (tokens.count == 0) { parsingError }

    var token = tokens.removeAt(0)

    if (token.type == JsonToken.LeftBrace) {
      // Making a Map
      var map = {}

      while (tokens[0].type != JsonToken.RightBrace) {
        var key = tokens.removeAt(0)
        if (key.type != JsonToken.String) { parsingError(key) }

        var next = tokens.removeAt(0)
        if (next.type != JsonToken.Colon) { parsingError(next) }

        var value = nest(tokens)
        map[key.value] = value

        if (tokens.count >= 2 &&
            tokens[0].type == JsonToken.Comma &&
            tokens[1].type != JsonToken.RightBrace) {
          tokens.removeAt(0)
        }
      }

      // Remove JsonToken.RightBrace
      tokens.removeAt(0)

      return map

    } else if (token.type == JsonToken.LeftBracket) {
      // Making a List
      var list = []
      while (tokens[0].type != JsonToken.RightBracket) {
        list.add(nest(tokens))

        if (tokens[0].type == JsonToken.Comma) {
          tokens.removeAt(0)
        }
      }

      // Remove JsonToken.RightBracket
      tokens.removeAt(0)

      return list

    } else if (valueTypes.contains(token.type)) {
      return token.value

    } else { parsingError(token) }
  }

  parsingError (token) {
    var position = Util.getPositionForIndex(_input, token.index)
    invalidJson("Unexpected \"%(token)\" at line %(position["line"]), column %(position["column"])")
  }

  parsingError {
    invalidJson("")
  }

  invalidJson(message) {
    var base = "Invalid Json"
    Fiber.abort(message.count > 0 ? "%(base): %(message)" : base)
  }
}

class JsonScanner {
  construct new(input) {
    _input = input
    _tokens = []
    // first unconsumed char
    _start = 0
    // char that will be considered next
    _cursor = 0
  }

  numberChars { "0123456789.-" }
  whitespaceChars { " \r\t\n"}
  escapedCharMap {
    return {
      "\"": "\"",
      "\\": "\\",
      "b": "\b",
      "f": "\f",
      "n": "\n",
      "r": "\r",
      "t": "\t"
    }
  }

  tokenize {
    while (!isAtEnd()) {
      _start = _cursor
      scanToken()
    }

    addToken(JsonToken.End)

    return _tokens
  }

  scanToken () {
    var char = advance()

    if (char == "{") {
      addToken(JsonToken.LeftBrace)
    } else if (char == "}") {
      addToken(JsonToken.RightBrace)
    } else if (char == "[") {
      addToken(JsonToken.LeftBracket)
    } else if (char == "]") {
      addToken(JsonToken.RightBracket)
    } else if (char == ":") {
      addToken(JsonToken.Colon)
    } else if (char == ",") {
      addToken(JsonToken.Comma)
    } else if (char == "/") {
      // Don't allow comments
      scanningError
    } else if (char == "\"") {
      scanString()
    } else if (numberChars.contains(char)) {
      scanNumber()
    } else if (isAlpha(char)) {
      scanIdentifier()
    } else if (whitespaceChars.contains(char)) {
      // pass
    } else {
      scanningError
    }
  }

  scanString () {
    var isEscaping = false
    var valueInProgress = []
    while ((peek() != "\"" || isEscaping) && !isAtEnd()) {
      var char = advance()
      if (isEscaping) {
        if (escapedCharMap.containsKey(char)) {
          valueInProgress.add(escapedCharMap[char])
        } else if (char == "u") { // unicode char!
          var charsToPull = 4
          var start = _cursor
          var hexString = _input.slice(start, start + charsToPull).join("")

          var decimal = Util.hexToDec(hexString)
          if (decimal == null) scanningError
          valueInProgress.add(String.fromCodePoint(decimal))
          _cursor = _cursor + charsToPull
        } else {
          scanningError
        }
        isEscaping = false
      } else if (char == "\\") {
        isEscaping = true
      } else {
        valueInProgress.add(char)
      }
    }

    if (isAtEnd()) {
      // unterminated string
      scanningError
      return
    }
    // consume closing "
    advance()
    addToken(JsonToken.String, valueInProgress.join(""))
  }

  scanNumber () {
    var value = String.fromCodePoint(_input.codePoints[_cursor - 1])
    while (numberChars.contains(peek())) {
      value = "%( value )%( advance() )"
    }
    var number = Num.fromString(value)
    if (number == null) {
      scanningError
    } else {
      addToken(JsonToken.Number, number)
    }
  }

  scanIdentifier () {
    var value = String.fromCodePoint(_input.codePoints[_cursor - 1])
    while (isAlpha(peek())) {
      value = "%( value )%( advance() )"
    }

    if (value == "true") {
      addToken(JsonToken.Bool, true)
    } else if (value == "false") {
      addToken(JsonToken.Bool, false)
    } else if (value == "null") {
      addToken(JsonToken.Null, null)
    } else {
      scanningError
    }
  }

  advance () {
    _cursor = _cursor + 1
    return String.fromCodePoint(_input.codePoints[_cursor - 1])
  }

  isAlpha (char) {
    var pt = char.codePoints[0]
    return (pt >= "a".codePoints[0] && pt <= "z".codePoints[0]) ||
           (pt >= "A".codePoints[0] && pt <= "Z".codePoints[0])
  }

  isAtEnd () {
    return _cursor >= _input.bytes.count
  }

  peek () {
    if (_input.codePoints[_cursor] < 0) { _cursor = _cursor + 1 }
    if (isAtEnd()) return "\0"
    return String.fromCodePoint(_input.codePoints[_cursor])
  }

  addToken(type) { addToken(type, null) }
  addToken(type, value) { _tokens.add(JsonToken.new(type, value, _cursor)) }

  scanningError {
    var value = _input.slice(_start, _cursor).join("")
    var position = Util.getPositionForIndex(_input, _start)
    Fiber.abort("Invalid JSON: Unexpected \"%(value)\" at line %(position["line"]), column %(position["column"])")
  }
}

class JsonToken {
  static LeftBracket { "LEFT_BRACKET" }
  static RightBracket { "RIGHT_BRACKET" }
  static LeftBrace { "LEFT_BRACE" }
  static RightBrace { "RIGHT_BRACE" }
  static Colon { "COLON" }
  static Comma { "COMMA" }
  static String { "STRING" }
  static Number { "NUMBER" }
  static Bool { "BOOL" }
  static Null { "NULL"}
  static End { "EOF"}

  construct new(type, value, index) {
    _type = type
    _value = value
    _index = index
  }

  toString {
    return (_value != null) ? (_type + " " + _value.toString) : _type
  }

  type { _type }
  value { _value }
  index { _index }
}

var HEX_CHARS = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F"]
var BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

class Util {

  foreign static randomString_(length)
  foreign static hash_(password)
  foreign static verify_(password, hash)

  static randomString(length) { randomString_(toNum(length)) }
  static hash(password) { hash_("%( password )") }
  static verify(password, hash) { verify_("%( password )", "%( hash )") }

  static toNum(val) {
    if (!val) return 0
    if (val is Num) return val
    val = Num.fromString("%(val)")
    if (!val) return 0
    return val
  }

  static hexToDec(hexStr) {
    var decimal = 0
    var length = hexStr.count
    var base = 1 // Initialize base value to 1, i.e., 16^0
    for (i in (length - 1)..0) {
      var char = hexStr[i].bytes[0]
      var value = 0
      if (char >= "0".bytes[0] && char <= "9".bytes[0]) {
        value = char - "0".bytes[0]
      } else if (char >= "A".bytes[0] && char <= "F".bytes[0]) {
        value = char - "A".bytes[0] + 10
      } else if (char >= "a".bytes[0] && char <= "f".bytes[0]) {
        value = char - "a".bytes[0] + 10
      }
      decimal = decimal + value * base
      base = base * 16
    }
    return decimal
  }

  static toHex(byte) {
    var hex = ""
    while (byte > 0) {
      var c = byte % 16
      hex = HEX_CHARS[c] + hex
      byte = byte >> 4
    }
    return hex
  }

  static urlDecode(str) {
    var decoded = ""
    var i = 0
    while (i < str.count) {
      if (str[i] == "\%") {
        var hex = str[i + 1..i + 2]
        var charCode = hexToDec(hex)
        decoded = decoded + String.fromByte(charCode)
        i = i + 3
      } else if (str[i] == "+") {
        decoded = decoded + " "
        i = i + 1
      } else {
        decoded = decoded + str[i]
        i = i + 1
      }
    }
    return decoded
  }

  static lpad(s, count, with) {
    while (s.count < count) {
      s = "%(with)%(s)"
    }
    return s
  }

  static reverse(str) {
    var result = ""
    for (char in str) {
      result = char + result
    }
    return result
  }

  static getPositionForIndex(text, index) {
    var precedingText = text.slice(0, index)
    var linebreaks = precedingText.where {|char| char == "\n"}

    var reversedPreceding = Util.reverse(precedingText)
    var hasSeenLinebreak = false
    var i = 0
    while (i < reversedPreceding.count && !hasSeenLinebreak) {
      if (reversedPreceding[i] == "\n") {
        hasSeenLinebreak = true
      }
      i = i + 1
    }

    return {
      "line": linebreaks.count,
      "column": i
    }
  }

  static htmlEscape(str) {
    if (!str) return ""
    return str.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\"", "&quot;")
  }

  static encodeBase64(input) {
    var encoded = ""
    var i = 0

    while (i < input.count) {
      i = i + 1
      var b1 = input.byteAt(i) & 0xFF
      if (i == input.count) {
        encoded = encoded + BASE64_CHARS[b1 >> 2]
        encoded = encoded + BASE64_CHARS[(b1 & 0x3) << 4]
        encoded = encoded + "=="
        break
      }

      i = i + 1
      var b2 = input.byteAt(i) & 0xFF
      if (i == input.count) {
        encoded = encoded + BASE64_CHARS[b1 >> 2]
        encoded = encoded + BASE64_CHARS[((b1 & 0x3) << 4) | ((b2 & 0xF0) >> 4)]
        encoded = encoded + BASE64_CHARS[(b2 & 0xF) << 2]
        encoded = encoded + "="
        break
      }

      i = i + 1
      var b3 = input.byteAt(i) & 0xFF
      encoded = encoded + BASE64_CHARS[b1 >> 2]
      encoded = encoded + BASE64_CHARS[((b1 & 0x3) << 4) | ((b2 & 0xF0) >> 4)]
      encoded = encoded + BASE64_CHARS[((b2 & 0xF) << 2) | ((b3 & 0xC0) >> 6)]
      encoded = encoded + BASE64_CHARS[b3 & 0x3F]
    }

    return encoded
  }

  static decodeBase64(input) {
    var decoded = ""
    var i = 0

    while (i < input.count) {
      var b1 = BASE64_CHARS.indexOf(input[i])
      i = i + 1
      var b2 = BASE64_CHARS.indexOf(input[i])
      i = i + 1
      var b3 = BASE64_CHARS.indexOf(input[i])
      i = i + 1
      var b4 = BASE64_CHARS.indexOf(input[i])
      i = i + 1

      decoded = decoded + String.fromByte((b1 << 2) | (b2 >> 4))
      if (b3 != -1) {
        decoded = decoded + String.fromByte(((b2 & 0xF) << 4) | (b3 >> 2))
        if (b4 != -1) {
          decoded = decoded + String.fromByte(((b3 & 0x3) << 6) | b4)
        }
      }
    }

    return decoded
  }
}

class Config {
  static get(key) { `SELECT val FROM BIALET_CONFIG WHERE key = ?`.first([key])["val"].toString }
  static set(key, value) { `REPLACE INTO BIALET_CONFIG (key, val) VALUES (?, ?)`.query(key, value) }
  static bool(key) { get(key) != "0" }
  static num(key) { Num.fromString(get(key)) }
  static delete(key) { `DELETE FROM BIALET_CONFIG WHERE key = ?`.first(key) }
  static json(key) { set(key, Json.parse(get(key))) }
  static json(key, val) { set(key, Json.stringify(val)) }
}

class Db {
  static init {
    `CREATE TABLE IF NOT EXISTS BIALET_MIGRATIONS (version TEXT, createdAt DATETIME DEFAULT CURRENT_TIMESTAMP)`.query()
    `CREATE TABLE IF NOT EXISTS BIALET_SESSION (id TEXT, key TEXT, val TEXT, updatedAt DATETIME)`.query()
    `CREATE TABLE IF NOT EXISTS BIALET_CONFIG (key TEXT PRIMARY KEY, val TEXT)`.query()
    `CREATE TABLE IF NOT EXISTS BIALET_USERS (id INTEGER PRIMARY KEY, email TEXT, password TEXT,
    name TEXT, isAdmin INTEGER, createdAt DATETIME DEFAULT CURRENT_TIMESTAMP, updatedAt DATETIME DEFAULT CURRENT_TIMESTAMP)`.query()
  }

  static migrate(version, schema) {
    Db.init
    if (!`SELECT version FROM BIALET_MIGRATIONS WHERE version = ?`.first([version])) {
      schema.toString.split(";").each{|q| Query.fromString(q, []) }
      `INSERT INTO BIALET_MIGRATIONS (version) VALUES (?)`.query([version])
    }
  }
  static save(table, values) {
    var bind = []
    var params = []
    for (v in values) {
      if (v.value is Query) {
        // add raw query string
        bind.add(v.value.toString)
      } else {
        bind.add("?")
        params.add(v.value)
      }
    }
    return Query.fromString("REPLACE INTO `%(table)` (%(values.keys.join(','))) VALUES (%(bind.join(',')))", params)
  }
}

class Http {
  construct new() {
    _method = false
    _basicAuth = ""
    _status = 0
    _headers = {}
    _body = ""
    _error = 0
    _fullHeaders = ""
    _postData = ""
  }
  body { _body }
  status { _status }
  headers { _headers }
  headers(name) { _headers.containsKey(name) ? _headers[name] : null }
  error { _error }
  method { _method }
  method=(m) { _method = m }
  postData=(data) {
    if (!_method) {
      _method = "POST"
    }
    _postData = data ? (data is String ? data : Json.stringify(data)) : ""
  }
  call(url, options) {
    if (!_method) {
      _method = "GET"
    }
    if (!options.containsKey("headers")) {
      options["headers"] = {}
    }
    if (!options["headers"].containsKey("Content-Type")) {
      options["headers"]["Content-Type"] = "application/json"
    }
    var headers = options["headers"].map{|h| "%(h.key): %(h.value)" }.join("\n")
    if (options["basicAuth"] != null) {
      _basicAuth = options["basicAuth"]["username"] + ":" + options["basicAuth"]["password"]
    }
    var response = call_(url, _method, headers, _postData, _basicAuth)

    if (response[1]) {
      var lines = response[1].split("\n")
      var tmp
      var headerName
      var headerValue
      for (line in lines) {
        tmp = line.split(":")
        headerName = tmp.removeAt(0).trim().lower
        headerValue = tmp.join(":").trim().lower
        _headers[headerName] = headerValue
      }
    }
    _status = response[0]
    _fullHeaders = response[1].trim()
    _body = response[2].trim()
    _error = response[3]

    return _error == 0
  }

  foreign call_(url, method, headers, postData, basicAuth)

  static request(url, method, data, options) {
    __http = Http.new()
    __http.method = method
    __http.postData = data
    if (!__http.call(url, options)) {
      return false
    }
    if (__http.status >= 200 && __http.status < 300) {
      if (__http.headers("content-type").contains("text/json") || __http.headers("content-type").contains("application/json")) {
        return Json.parse(__http.body)
      } else {
        return __http.body
      }
    }
  }
  // Shortcuts for common HTTP methods
  static get(url, options) { request(url, "GET", null, options) }
  static post(url, data, options) { request(url, "POST", data, options) }
  static put(url, data, options) { request(url, "PUT", data, options) }
  static delete(url, options) { request(url, "DELETE", null, options) }
  static get(url) { get(url, {}) }
  static post(url, data) { post(url, data, {}) }
  static post(url) { post(url, "", {}) }
  static put(url, data) { put(url, data, {}) }
  static put(url) { put(url, "", {}) }
  static delete(url) { delete(url, {}) }
}

// @TODO Use a C library instead of calling to SQLite in the Date class
class Date {
  construct new() {
    _utc = __utc
    _date = Date.d2U_("now", _utc)
  }
  construct new(date) {
    _utc = __utc
    _date = Date.d2U_(date, _utc)
  }
  construct new(date, utc) {
    _utc = utc
    _date = Date.d2U_(date, _utc)
  }
  construct new(year, month, day, hour, minute, second) {
    _utc = __utc
    _date = Date.d2U_("%(year)-%(month)-%(day) %(hour):(minute):%(second)", _utc)
  }
  construct new(year, month, day, hour, minute, second, utc) {
    _utc = utc
    _date = Date.d2U_("%(year)-%(month)-%(day) %(hour):(minute):%(second)", _utc)
  }
  construct new(year, month, day) {
    _utc = __utc
    _date = Date.d2U_("%(year)-%(month)-%(day) 00:00:00", _utc)
  }
  construct new(year, month, day, utc) {
    _utc = utc
    _date = Date.d2U_("%(year)-%(month)-%(day) 00:00:00", _utc)
  }
  // Save the date in UTC
  static d2U_(date, utc) {
    if (utc == 0) return date
    utc = utc * -1
    var a = "%(utc):00"
    if (utc > 0 && utc < 10) a = "+0%(utc):00"
    if (utc > -10 && utc < 0) a = "-0%(utc.abs):00"
    if (utc > 10) a = "+%(utc):00"
    return `SELECT strftime('%Y-%m-%d %H:%M:%S', ?, ?)`.val(date, a)
  }
  static now { Date.new() }
  static utc_(utc) { utc >= 0 ? "+%(utc)" : utc }
  static utc=(utc) { utc is Num ? __utc = utc : __utc = Num.fromString(utc.upper.replace("UTC", "")) }
  static utc { Date.utc_(__utc) }
  utc=(utc) { utc is Num ? _utc = utc : _utc = Num.fromString(utc.upper.replace("UTC", "")) }
  utc { Date.utc_(_utc) }
  // Modifier to adjust the date relative to UTC
  a {
    if (_utc >= 0 && _utc < 10) return "+0%(_utc):00"
    if (_utc > -10 && _utc < 0) return "-0%(_utc.abs):00"
    if (_utc > 10) return "+%(_utc):00"
    return "%(_utc):00"
  }
  format(format) { `SELECT strftime(?, ?, ?)`.val(format.replace("#", "\%"), _date, a) }
  year { `SELECT strftime('%Y', ?, ?)`.toNumber(_date, a) }
  month { `SELECT strftime('%m', ?, ?)`.toNumber(_date, a) }
  day { `SELECT strftime('%e', ?, ?)`.toNumber(_date, a) }
  hour { `SELECT strftime('%k', ?, ?)`.toNumber(_date, a) }
  minute { `SELECT strftime('%M', ?, ?)`.toNumber(_date, a) }
  second { `SELECT strftime('%S', ?, ?)`.toNumber(_date, a) }
  weekday { `SELECT strftime('%w', ?, ?)`.toNumber(_date, a) }
  dayOfYear { `SELECT strftime('%j', ?, ?)`.toNumber(_date, a) }
  date { `SELECT strftime('%Y-%m-%d', ?, ?)`.val(_date, a) }
  time { `SELECT strftime('%H:%M:%S', ?, ?)`.val(_date, a) }
  unix { `SELECT strftime('%s', ?, ?)`.toNumber(_date, a) }
  iso { `SELECT strftime('%T', ?, ?)`.toString(_date, a) }
  inUtc { _date }
  toString { `SELECT strftime('%Y-%m-%dT%H:%M:%S', ?, ?)`.val(_date, a) }
  +(plus) { _date = `SELECT strftime('%Y-%m-%dT%H:%M:%S', ?, ?)`.val(_date, plus, a) }
  -(minus) { _date = `SELECT strftime('%Y-%m-%dT%H:%M:%S', ?, ?)`.val(_date, "-%(minus)", a) }
  diff(otherDate) { `SELECT timediff(?, ?)`.val(_date, otherDate.inUtc) }
  cmp_(o) {
    var diff = diff(o)
    if (diff == "+0000-00-00 00:00:00.000") return 0
    if (diff[0] == "-") return -1
    if (diff[0] == "+") return 1
    Fiber.abort("Unexpected date diff: %(diff)")
  }
  < (o) { cmp_(o) <  0 }
  > (o) { cmp_(o) >  0 }
  <=(o) { cmp_(o) <= 0 }
  >=(o) { cmp_(o) >= 0 }
  ==(o) { cmp_(o) == 0 }
  !=(o) { cmp_(o) != 0 }
}
// Set the Global UTC to 0
Date.utc = 0

// Setup
Response.init()
Cookie.init()
