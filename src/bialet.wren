/*
 * This file is part of Bialet, which is licensed under the
 * GNU General Public License, version 2 (GPL-2.0).
 *
 * Copyright (c) 2023 Rodrigo Arce
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * For full license text, see LICENSE.md.
 */
class Response {
  static init() {
    __headers = {"Content-Type": "text/html"}
    __cookies = []
    __status = 200
    __out = ""
  }
  static out(out) { __out = __out + "\r\n" + out }
  static out() { __out.trim() }
  static status(status) { __status = status }
  static status() { __status }
  static headers() { __cookies.join("\r\n") +  __headers.keys.map{|k| k + ": " + __headers[k] + "\r\n"}.join() }
  static addCookieHeader(value) { __cookies.add("Set-Cookie: %(value)") }
  static header(header, value) { __headers[header.trim()] = value.trim() }
  static redirect(url) {
    Response.status(302)
    Response.header("Location", url)
  }
  static json(data) {
    Response.header("Content-Type", "application/json")
    Response.out(Json.stringify(data))
  }
}

class User {
  foreign static hash(password)
  foreign static verify(password, hash)
}

var HEX_CHARS = [
  "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F"
]

class Util {

  foreign static randomString(length)

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

  static slice(list, start) {
    return slice(list, start, list.count)
  }
  static slice(list, start, end) {
    var result = []
    for (index in start...end) {
      result.add(list[index])
    }
    return result
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
  static lpad(s, count, with) {
    while (s.count < count) {
      s = "%(with)%(s)"
    }
    return s
  }
  static reverse (str) {
    var result = ""
    for (char in str) {
      result = char + result
    }
    return result
  }
  static getPositionForIndex (text, index) {
    var precedingText = Util.slice(text, 0, index)
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
}

class Html {
  static escape(str) {
    if (!str) return ""
    return str.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\"", "&quot;")
  }
}

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
        headerName = tmp.removeAt(0).trim()
        headerValue = tmp.join(":").trim()
        if (headerName.lower == "cookie") {
          Cookie.init(headerValue)
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
    /* @TODO Support arrays in URL and POST query */
    query.split("&").each{|q|
      var value = ""
      var tmp = q.split("=")
      var key = Util.urlDecode(tmp[0])
      if (tmp.count > 1) {
        value = Util.urlDecode(tmp[1])
      }
      all[key] = value
    }
    return all
  }
  static header(name) { __headers[name] ? __headers[name]:"" }
  static get(name) { __get[name] ? __get[name]:"" }
  static post(name) { __post[name] ? __post[name]:"" }
  static route(pos) { __route.count > pos ? __route[pos]:"" }
  static method() { __method }
  static uri() { __uri }
  static body() { __body }
  static isPost() { __method == "POST" }
}

class Cookie {
  static init() { __cookies = {} }
  static init(cookieLine) {
    var cookies = cookieLine.split(";")
    __cookies = {}
    for (cookie in cookies) {
      var tmp = cookie.split("=")
      var name = tmp[0].trim()
      var value = tmp[1].trim()
      __cookies[name] = value
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
    Db.query("DELETE FROM BIALET_SESSION WHERE id = ? OR updatedAt < date('now', '-1 month')", [id])
  }
  construct new() {
    _id = Cookie.get(Session.name)
    if (!_id) {
      _id = Util.randomString(40)
      Cookie.set(Session.name, _id)
    }
    __values = {}
    var res = Db.all("SELECT key, val FROM BIALET_SESSION WHERE id = ?", [_id])
    if (res && res.count > 0) {
      res.each{|r| __values[r["key"]] = r["val"] }
    }
  }
  id { _id }
  get(key) { __values[key] ? __values[key] : null }
  set(key, value) {
    __values[key] = value
    Db.query("REPLACE INTO BIALET_SESSION (id, key, val, updatedAt) VALUES (?, ?, ?, CURRENT_TIMESTAMP)", [_id, key, "%(value)"])
  }
}

class Db {

  static count{ 0 }
  static map(callback){ }

  foreign static intQuery(query, params)
  foreign static intLastInsertId()
  static query(query, params){
    if (!query || query.trim() == "") {
      return []
    }
    return Db.intQuery(query, params)
  }
  static lastInsertId(){ intLastInsertId() }
  static migrate(version, schema) {
    Db.query("CREATE TABLE IF NOT EXISTS BIALET_MIGRATIONS (version TEXT, createdAt DATETIME DEFAULT CURRENT_TIMESTAMP)")
    Db.query("CREATE TABLE IF NOT EXISTS BIALET_SESSION (id TEXT, key TEXT, val TEXT, updatedAt DATETIME)")
    if (!Db.one("SELECT version FROM BIALET_MIGRATIONS WHERE version = ?", [version])) {
      schema.split(";").each{|q| Db.query(q) }
      Db.query("INSERT INTO BIALET_MIGRATIONS (version) VALUES (?)", [version])
    }
  }
  static query(query) { Db.query(query, []) }
  static all(query) { Db.query(query, []) }
  static all(query, params) { Db.query(query, params) }
  static one(query, params) {
    var res = Db.all(query + " limit 1", params)
    if (res && res.count > 0) {
      return res[0]
    }
    return null
  }
  static one(query) { Db.one(query, []) }
  static save(table, values) {
    Db.all("REPLACE INTO " + table + "(" + values.keys.join(", ") + ") VALUES (" + values.map{|v| "?"}.join(", ") + ")", values.values.toList)
    return Db.lastInsertId()
  }
}

class Http {
  construct new() {
    _method = false
    _basicAuth = ""
  }
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
    System.print("Request: %(url) %(options)")
    var headers = options["headers"].map{|h| "%(h.key): %(h.value)" }.join("\n")
    if (options["basicAuth"] != null) {
      _basicAuth = options["basicAuth"]["username"] + ":" + options["basicAuth"]["password"]
    }
    var response = intCall(url, _method, headers, _postData, _basicAuth)
    return {"status": response[0], "headers": response[1], "body": response[2]}
  }
  foreign intCall(url, method, headers, postData, basicAuth)
  static request(url, method, data, options) {
    __http = Http.new()
    __http.method = method
    __http.postData = data
    var response = __http.call(url, options)
    System.print("Response: %(response)")
    if (response["body"] != "") {
      return Json.parse(response["body"])
    }
    return false
  }
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
    if (obj is Num || obj is Bool || obj is Null) {
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

  valueTypes { [Token.String, Token.Number, Token.Bool, Token.Null] }

  parse { nest(JsonScanner.new(_input).tokenize) }

  nest(tokens) {
    if (tokens.count == 0) { parsingError }

    var token = tokens.removeAt(0)

    if (token.type == Token.LeftBrace) {
      // Making a Map
      var map = {}

      while (tokens[0].type != Token.RightBrace) {
        var key = tokens.removeAt(0)
        if (key.type != Token.String) { parsingError(key) }

        var next = tokens.removeAt(0)
        if (next.type != Token.Colon) { parsingError(next) }

        var value = nest(tokens)
        map[key.value] = value

        if (tokens.count >= 2 &&
            tokens[0].type == Token.Comma &&
            tokens[1].type != Token.RightBrace) {
          tokens.removeAt(0)
        }
      }

      // Remove Token.RightBrace
      tokens.removeAt(0)

      return map

    } else if (token.type == Token.LeftBracket) {
      // Making a List
      var list = []
      while (tokens[0].type != Token.RightBracket) {
        list.add(nest(tokens))

        if (tokens[0].type == Token.Comma) {
          tokens.removeAt(0)
        }
      }

      // Remove Token.RightBracket
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

    addToken(Token.End)

    return _tokens
  }

  scanToken () {
    var char = advance()

    if (char == "{") {
      addToken(Token.LeftBrace)
    } else if (char == "}") {
      addToken(Token.RightBrace)
    } else if (char == "[") {
      addToken(Token.LeftBracket)
    } else if (char == "]") {
      addToken(Token.RightBracket)
    } else if (char == ":") {
      addToken(Token.Colon)
    } else if (char == ",") {
      addToken(Token.Comma)
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
          var hexString = Util.slice(_input, start, start + charsToPull).join("")

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

    addToken(Token.String, valueInProgress.join(""))
  }

  scanNumber () {
    while (numberChars.contains(peek())) {
      advance()
    }

    var number = Num.fromString(Util.slice(_input, _start, _cursor).join(""))

    if (number == null) {
      scanningError
    } else {
      addToken(Token.Number, number)
    }
  }

  scanIdentifier () {
    while (isAlpha(peek())) {
      advance()
    }

    var value = Util.slice(_input, _start, _cursor).join("")
    if (value == "true") {
      addToken(Token.Bool, true)
    } else if (value == "false") {
      addToken(Token.Bool, false)
    } else if (value == "null") {
      addToken(Token.Null, null)
    } else {
      scanningError
    }
  }

  advance () {
    _cursor = _cursor + 1
    return _input[_cursor - 1]
  }

  isAlpha (char) {
    var pt = char.codePoints[0]
    return (pt >= "a".codePoints[0] && pt <= "z".codePoints[0]) ||
           (pt >= "A".codePoints[0] && pt <= "Z".codePoints[0])
  }

  isAtEnd () {
    return _cursor >= _input.count
  }

  peek () {
    if (isAtEnd()) return "\0"
    return _input[_cursor]
  }

  addToken(type) { addToken(type, null) }
  addToken(type, value) { _tokens.add(Token.new(type, value, _cursor)) }

  scanningError {
    var value = Util.slice(_input, _start, _cursor).join("")
    var position = Util.getPositionForIndex(_input, _start)
    Fiber.abort("Invalid JSON: Unexpected \"%(value)\" at line %(position["line"]), column %(position["column"])")
  }
}

class Token {
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

Cookie.init()
Response.init()
