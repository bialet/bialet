//
// This file is part of Bialet, which is licensed under the
// MIT License.
//
// Copyright (c) 2023-2026 Rodrigo Arce
//
// SPDX-License-Identifier: MIT
//
// For full license text, see LICENSE.md.

class Request {
  static init(message, route, files, fields) {
    __message = message
    __headers = {}
    __get = {}
    __post = {}
    __files = files
    var lines = message.split("\n")
    var tmp = lines.removeAt(0).split(" ")
    __method = tmp[0].upper
    __fullUri = tmp[1]
    __body = ""
    // Split the query off before the path is segmented: /notes?page=2 is the
    // route /notes with a parameter, not a segment called "?page=2".
    var uriSeparator = __fullUri.indexOf("?")
    if (uriSeparator > 0) {
      __uri = __fullUri[0...uriSeparator]
      __get = parseQuery(__fullUri[uriSeparator+1...__fullUri.count])
    } else {
      __uri = __fullUri
    }
    var routeRest = (route.count > 0 && __uri.startsWith(route)) ? __uri[route.count..-1] : __uri
    __segments = routeRest.trimStart("/").split("/")
    var startBody = false
    var headerName
    var headerValue
    var bodyLines = []
    for (line in lines) {
      if (startBody) {
        bodyLines.add(line)
        continue
      }
      if (line.trim() == "") {
        startBody = true
        continue
      }
      tmp = line.split(":")
      // A line without ':' is not a header field; the C scanner never matches
      // it, so it must not overwrite an earlier value with "".
      if (tmp.count < 2) continue
      var rawName = tmp.removeAt(0)
      // RFC 7230 forbids whitespace before the colon. Skipping such a line
      // keeps the app-visible map in step with the C multipart detector, which
      // never matches a name followed by anything but ':'.
      if (rawName != rawName.trim()) continue
      headerName = rawName.trim().lower
      headerValue = tmp.join(":").trim()
      if (headerName == "cookie") {
        Cookie.parseHeader(headerValue)
      }
      __headers[headerName] = headerValue
    }
    __body = bodyLines.join("")
    if (__method == "POST") {
      __post = fields != "" ? parseFields_(fields) : parseQuery(__body)
    }
  }

  // The text fields of a multipart body, extracted by the server as
  // percent-encoded "name=value" pairs (every pair has its "="). File parts are
  // not included: they are read with Request.file.
  static parseFields_(encoded) {
    var all = {}
    for (pair in encoded.split("&")) {
      var eq = pair.indexOf("=")
      if (eq < 0) continue
      all[Util.urlDecode(pair[0...eq])] = Util.urlDecode(pair[eq + 1...pair.count])
    }
    return all
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
  static json() { Json.parse(__body) }
  static header(name) { __headers[name] ? __headers[name]:null }
  static get(name) { __get[name] ? __get[name]:null }
  static query(name) { get(name) }
  static post(name) { __post[name] ? __post[name]:null }
  static route(pos) { __segments.count > pos && __segments[pos] != "" ? __segments[pos]:null}
  static file(name) {
    if (!__files || __files.trim() == "") return null
    for (fileId in __files.split(",")) {
      fileId = fileId.trim()
      if (fileId == "") continue
      if (!Util.isDigits(fileId)) Fiber.abort("Invalid uploaded file id")
      var file = `SELECT * FROM BIALET_FILES WHERE name = ? AND id = ?`.first([name, fileId])
      // Reading an upload does not keep it: the row stays isTemp = 1 and inside
      // the aggregate pool until the handler calls .save (or .destroy). The old
      // getter called .save, so simply inspecting a file - including to reject
      // it - moved it out of the ceiling and the purge forever.
      if (file) return File.new(file)
    }
    return null
  }
  static login(user, pass) {
    var authHeader = Request.header("authorization")
    if (!authHeader) return Response.login()
    var parts = authHeader.split(" ")
    if (parts.count != 2 || parts[0].lower != "basic") return Response.login()
    // A real Basic credential is a few dozen characters. Refuse an
    // attacker-chosen length before spending any decoding work on it; the
    // header block is the only other bound on this value.
    if (parts[1].count > 4096) return Response.login()
    var decoded = Util.decodeBase64(parts[1])
    if (!decoded) return Response.login()
    var separator = decoded.indexOf(":")
    if (separator < 0) return Response.login()
    var loginUser = decoded[0...separator]
    var loginPass = decoded[separator + 1..-1]
    if (!Util.secureEquals(loginUser, user) || !Util.secureEquals(loginPass, pass)) {
      return Response.login()
    }
    return false
  }
}

class Response {
  static init {
    __headers = {"Content-Type": "text/html; charset=UTF-8"}
    __cookies = []
    __status = 200
    __out = ""
    __useErrorFallback = false
    __fileId = null
    Cookie.init
  }

  // Getters
  static out { __out.trim() }
  static status { __status }
  // Each cookie and header is its own "\r\n"-terminated line. Concatenating
  // __cookies.join("\r\n") directly with the first header glued the last
  // cookie's final attribute onto the header (e.g. "Path=/Content-Type: ..."),
  // corrupting cookie attributes such as Path. Keep this single-line: this
  // Wren fork mis-parses a getter body whose binary operator ends a line.
  static headers { __cookies.map{|c| c + "\r\n"}.join() + __headers.keys.map{|k| k + ": " + __headers[k] + "\r\n"}.join() }

  static out(out) { __out = __out + "\r\n" + out.toString }
  static status(status) { __status = status }
  static useErrorFallback() { __useErrorFallback }
  static addCookieHeader(value) { __cookies.add("Set-Cookie: %(Util.headerValue(value))") }
  static header(header, value) { __headers[Util.headerName(header)] = Util.headerValue(value) }
  // Out of band: the id of the blob Response.file selected, or null. The id
  // used to live in the body as a leading 0x1A marker, which client-influenced
  // output could forge to read an arbitrary BIALET_FILES row.
  static fileId { __fileId }
  static file(id) {
    var type = `SELECT type FROM BIALET_FILES WHERE id = ?`.val([id])
    if (!type) return false
    var numeric = id is Num ? id : Num.fromString("%(id)")
    if (!numeric) return false
    __fileId = numeric
    __out = ""
    header("Content-Type", type)
    return true
  }

  static json(data) {
    header("Content-Type", "application/json; charset=UTF-8")
    var body = Json.stringify(data)
    __out = __out + "\r\n" + body
    return body
  }

  static cors(origin, methods, headers) {
    header("Access-Control-Allow-Origin", origin)
    header("Access-Control-Allow-Methods", methods)
    header("Access-Control-Allow-Headers", headers)
    if (Request.method == "OPTIONS") {
      status(204)
      return true
    }
    return false
  }
  static cors(origin) { cors(origin, "GET, POST, PUT, DELETE, OPTIONS", "Content-Type, Authorization") }
  static cors() { cors("*") }
  static cors { cors("*") }

  static page(title, message) { pageHtml(title, Util.htmlEscape(message)) }
  // defaultPage_ is a C native that renders the same page chrome as the
  // server's C-side fallbacks (see BIALET_HEADER_PAGE/BIALET_FOOTER_PAGE in
  // bialet.h), so the default pages live in one place instead of two.
  static pageHtml(title, messageHtml) { defaultPage_(Util.htmlEscape(title), messageHtml) }
  static end(code, title, message) {
    status(code)
    var content = page(title, message)
    __out = __out + "\r\n" + content
    return content
  }
  static endHtml(code, title, messageHtml) {
    status(code)
    var content = pageHtml(title, messageHtml)
    __out = __out + "\r\n" + content
    return content
  }

  static redirect(url) {
    header("Location", url)
    var safeUrl = Util.htmlEscape(url)
    return endHtml(302, "➡️ Redirect to", '<a href="%(safeUrl)">%(safeUrl)</a>')
  }
  static forbidden() {
    __useErrorFallback = true
    end(403, "🚫 Forbidden", "Sorry, you don't have permission to access this page")
  }
  static notFound() {
    __useErrorFallback = true
    end(404, "⚠️ Not found", "Uh-oh! No route found.")
  }
  static internalError() {
    __useErrorFallback = true
    end(500, "🚨 Internal Server Error", "Oops! Something broke.")
  }
  static payloadTooLarge() {
    __useErrorFallback = true
    end(413, "📦 Payload Too Large", "The request body is too large.")
  }
  static tooManyRequests() {
    __useErrorFallback = true
    end(429, "⏳ Too Many Requests", "Please slow down and try again later.")
  }
  static login() {
    header("WWW-Authenticate", 'Basic realm="Login required"')
    return endHtml(401, "🔒 Needs login", '<a href="javascript:location.reload()">Sign in</a> to access this page')
  }
}

class Cookie {
  static init { __cookies = {} }
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
    var safeName = Util.cookieToken(name, "Cookie name")
    var safeValue = Util.cookieValue(value, "Cookie value")
    var cookieOptions = {
      "Path": "/",
      "HttpOnly": true,
      "SameSite": "Lax"
    }
    if (Util.isHttpsRequest) {
      cookieOptions["Secure"] = true
    }
    for (key in options.keys) {
      cookieOptions[key] = options[key]
    }
    var serialized = []
    for (key in cookieOptions.keys) {
      var safeKey = Util.cookieToken(key, "Cookie option")
      var optionValue = cookieOptions[key]
      if (optionValue == null || optionValue == false) continue
      if (optionValue == true) {
        serialized.add(safeKey)
      } else {
        serialized.add("%(safeKey)=%(Util.cookieValue(optionValue, safeKey))")
      }
    }
    Response.addCookieHeader("%(safeName)=%(safeValue); %(serialized.join("; "))")
    __cookies[safeName] = safeValue
  }
  static set(name, value){ set(name, value, {}) }
  static delete(name){ set(name, "", {"expires": "Thu, 01 Jan 1970 00:00:00 GMT"}) }
  static get(name, default){ __cookies != null && __cookies[name] ? __cookies[name]: default }
  static get(name){ get(name, null) }
}

class Session {
  static name { __name ? __name : "BIALETSESSID" }
  static name=(n) { __name = n }
  // BIALET_SESSION is written by unauthenticated requests: a client that drops
  // its cookie (or presents a fresh valid id) adds a row per request. Bound the
  // table the way the temp-upload pool is bounded, and expire on the write path
  // instead of only from Db.migrate at startup.
  static maxSessions { __maxSessions ? __maxSessions : 32768 }
  static maxSessions=(n) { __maxSessions = n }
  // bialet_run() builds and frees a Wren VM for every request, so the old
  // amortization counter here restarted at 0 each time and could never reach
  // its threshold: for one-write-per-request traffic (what Session.csrf
  // produces) the sweep never ran at all. Draw per write instead - about one
  // write in 62, the same shape PHP uses for session GC - so the trigger does
  // not depend on the VM or request lifetime.
  static sweepDue_ { Util.randomString(1).bytes[0] == 97 }
  static sweep_() {
    // Cap each statement: a backlog built up while the trigger was broken must
    // not block the single worker in one pass. Later sweeps make progress.
    var batch = 1000
    // An id expires when it has been idle for 30 days: the age test has to be
    // on MAX(updatedAt) per id, not on individual rows. rotate() writes the
    // _bialet_authenticated marker once and nothing refreshes it, so testing
    // rows deleted an actively used session on day 31 by its marker row.
    `DELETE FROM BIALET_SESSION WHERE id IN (SELECT id FROM BIALET_SESSION GROUP BY id HAVING MAX(updatedAt) < datetime('now', '-30 days') LIMIT ?)`.query(batch)
    var row = `SELECT COUNT(*) AS c FROM (SELECT id FROM BIALET_SESSION GROUP BY id)`.first()
    if (!row) return
    var live = Num.fromString("%(row["c"])")
    if (!live) return
    var excess = live - Session.maxSessions
    if (excess > 0) {
      if (excess > batch) excess = batch
      // The cap may only evict sessions that were never authenticated. A
      // flood adds one fresh id per request, so the oldest rows are the
      // logged-in users' login-time rows, and deleting those logs them out
      // globally. rotate() marks the id it mints, so the candidates here are
      // the ids without that marker; a session that was authenticated stays
      // until the idle expiry above removes it. The inner NOT IN is guarded
      // against a NULL id: SQLite allows one in a composite text key, and a
      // single NULL would make NOT IN unknown for every row and silently
      // disable the cap.
      `DELETE FROM BIALET_SESSION WHERE id IN (SELECT id FROM BIALET_SESSION WHERE id NOT IN (SELECT id FROM BIALET_SESSION WHERE key = '_bialet_authenticated' AND id IS NOT NULL) GROUP BY id ORDER BY MAX(updatedAt) ASC LIMIT ?)`.query(excess)
    }
  }
  static destroy() {
    var id = Cookie.get(Session.name)
    Cookie.delete(Session.name)
    `DELETE FROM BIALET_SESSION WHERE id = ? OR updatedAt < date('now', '-1 month')`.query([id])
  }
  // A session identifier must look like one: non-empty, at most 128 bytes, and
  // URL-safe. The server only ever mints 40 random characters, so anything else
  // is either a client-chosen value (fixation) or a truncated/oversized cookie.
  static validId_(id) {
    if (id == null) return false
    if (id.count == 0 || id.count > 128) return false
    for (char in id) {
      var b = char.bytes[0]
      var ok = (b >= 48 && b <= 57) || (b >= 65 && b <= 90) || (b >= 97 && b <= 122) ||
               b == 95 || b == 45
      if (!ok) return false
    }
    return true
  }
  // Mint a fresh identifier and drop the old session's rows. Call this after a
  // successful login so a session identifier planted before authentication is
  // worthless afterwards; the framework has no login hook of its own.
  static rotate() {
    var old = Cookie.get(Session.name)
    if (Session.validId_(old)) {
      `DELETE FROM BIALET_SESSION WHERE id = ?`.query([old])
    }
    var id = Util.randomString(40)
    Cookie.set(Session.name, id)
    // Mark the minted id as authenticated, so sweep_() can tell a logged-in
    // session from the anonymous rows a flood creates. Written through set()
    // so it follows the live cookie like every other key.
    Session.new().set("_bialet_authenticated", "1")
    return id
  }
  construct new() {
    _id = Cookie.get(Session.name)
    if (!Session.validId_(_id)) {
      _id = Util.randomString(40)
      Cookie.set(Session.name, _id)
    }
    __values = {}
    var res = `SELECT key, val FROM BIALET_SESSION WHERE id = ? ORDER BY updatedAt DESC`.fetch([_id])
    if (res && res.count > 0) {
      res.each{|r| __values[r["key"]] = r["val"] }
    }
  }
  static id { Session.new().id }
  id { _id }
  get(key) { __values[key] ? __values[key] : null }
  set(key, value) {
    // rotate() earlier in this request retired the identifier this instance was
    // built with, and destroy() may have cleared the cookie: follow the live
    // cookie instead of the stale copy, or a planted pre-login identifier keeps
    // receiving authenticated state after rotate() deleted its row.
    var current = Cookie.get(Session.name)
    if (!Session.validId_(current)) {
      current = Util.randomString(40)
      Cookie.set(Session.name, current)
    }
    if (current != _id) {
      _id = current
      __values = {}
    }
    __values[key] = value
    `REPLACE INTO BIALET_SESSION (id, key, val, updatedAt) VALUES (?, ?, ?, CURRENT_TIMESTAMP)`.query(_id, key, "%(value)")
    // Amortize the sweep: an unauthenticated flood can add one row per request,
    // and a count+evict on every write would itself be work for that flood.
    // The trigger cannot live in a class variable here - that state is per-VM
    // and this code runs in a fresh VM per request.
    if (Session.sweepDue_) {
      Session.sweep_()
    }
  }
  csrf {
    var token = get("_bialet_csrf")
    if (!token) {
      token = Util.randomString(60)
      set("_bialet_csrf", token)
    }
    return HtmlNode.new('<input type="hidden" name="_bialet_csrf" value="%( Util.htmlEscape(token) )">')
  }
  csrfOk { Util.secureEquals(get("_bialet_csrf"), Request.post("_bialet_csrf")) }
  // Statics so a handler or template can write Session.csrf / Session.csrfOk
  // without holding an instance. Each call builds a session from the live
  // cookie, so the token rendered by a GET is the one a later POST validates.
  static csrf { Session.new().csrf }
  static csrfOk { Session.new().csrfOk }
}

// Json library and Util functions are from Matthew Brandly
// https://github.com/brandly/wren-json
class Json {
  static parse(string) {
    return parse_(string)
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

  static htmlEscape(value) {
    var input = value == null ? "" : "%(value)"
    var escaped = []
    for (char in input) {
      if (char == "&") {
        escaped.add("&amp;")
      } else if (char == "<") {
        escaped.add("&lt;")
      } else if (char == ">") {
        escaped.add("&gt;")
      } else if (char == "\"") {
        escaped.add("&quot;")
      } else if (char == "'") {
        escaped.add("&#x27;")
      } else {
        escaped.add(char)
      }
    }
    return escaped.join("")
  }

  static isDigits(value) {
    if (!value || value.count == 0) return false
    for (char in value) {
      var byte = char.bytes[0]
      if (byte < "0".bytes[0] || byte > "9".bytes[0]) return false
    }
    return true
  }

  // Constant-time byte comparison. String.count walks code points while
  // String[i] indexes bytes, so the old loop stopped before the end of a
  // multi-byte value and compared only each character's lead byte: "é"/"à",
  // "aéX"/"aéY" and "contraseña"/"contraseñX" were all reported equal. The
  // native side compares raw bytes and folds the length into the result.
  static secureEquals(left, right) {
    if (left == null || right == null) return false
    return secureEquals_("%(left)", "%(right)")
  }

  static hasHeaderBreak(value) {
    value = value == null ? "" : "%(value)"
    for (char in value) {
      var byte = char.bytes[0]
      if (byte < 32 || byte == 127) return true
    }
    return false
  }

  static stripControlChars(value) {
    value = value == null ? "" : "%(value)"
    var sanitized = []
    for (char in value) {
      var byte = char.bytes[0]
      if (byte >= 32 && byte != 127) {
        sanitized.add(char)
      }
    }
    return sanitized.join("")
  }

  static headerName(value) {
    value = value == null ? "" : "%(value)"
    var header = value.trim()
    if (header == "" || hasHeaderBreak(header) || header.contains(":")) {
      Fiber.abort("Invalid response header name")
    }
    return header
  }

  static headerValue(value) {
    return stripControlChars(value).trim()
  }

  static cookieToken(value, label) {
    value = value == null ? "" : "%(value)"
    var token = value.trim()
    if (token == "" || hasHeaderBreak(token)) Fiber.abort("%(label) is invalid")
    for (char in token) {
      if (char == " " || char == "\t" || char == ";" || char == "=" || char == ",") {
        Fiber.abort("%(label) contains invalid characters")
      }
    }
    return token
  }

  static cookieValue(value, label) {
    value = stripControlChars(value)
    if (value.contains(";")) {
      Fiber.abort("%(label) contains invalid characters")
    }
    return value
  }

  static isHttpsRequest {
    var forwardedProto = Request.header("x-forwarded-proto")
    if (forwardedProto && forwardedProto.lower == "https") return true
    var forwarded = Request.header("forwarded")
    return forwarded && forwarded.lower.contains("proto=https")
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

  static urlDecode(str) { urlDecode_("%(str)") }
  static monotonicMs { monotonicMs_() }

  static urlEncode(str) {
    var parts = []
    str = "%(str)"
    for (char in str) {
      if (char == " ") {
        parts.add("+")
      } else if (char == "\%") {
        parts.add("\%25")
      } else if (char == "&") {
        parts.add("\%26")
      } else if (char == "=") {
        parts.add("\%3D")
      } else if (char == "?") {
        parts.add("\%3F")
      } else {
        parts.add(char)
      }
    }
    return parts.join("")
  }

  static params(params) {
    var result = ""
    var first = true
    for (entry in params) {
      if (!first) {
        result = result + "&"
      }
      result = result + urlEncode(entry.key) + "=" + urlEncode(entry.value)
      first = false
    }
    return result
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

  static sha256(input) { sha256_("%( input )") }

  // BASE64URL(SHA256(input)) without padding: PKCE's code_challenge for S256.
  static sha256Base64Url(input) { sha256Base64Url_("%( input )") }

  // RFC 4648 §5: URL-safe alphabet, no padding.
  static base64UrlEncode(input) { base64UrlEncode_("%( input )") }

  // Standard alphabet with '=' padding; round-trips with decodeBase64.
  static encodeBase64(input) { encodeBase64_("%( input )") }

  // Decodes standard Base64 (RFC 4648 §4). Returns null when the value is not
  // valid Base64: the old loop read four characters per group without checking
  // the length, so a truncated value (a garbled Authorization header) indexed
  // past the end and aborted the request with a 500 instead of letting the
  // caller answer 401. The final group may omit its '=' padding.
  static decodeBase64(input) {
    var count = input.count
    // One leftover character cannot encode a byte.
    if (count % 4 == 1) return null

    // Collect into a list and join once: appending with `+` per decoded byte
    // copied the whole accumulated prefix, so one long value cost Theta(n^2)
    // and could stall the single HTTP worker before any credential check.
    var out = []
    var i = 0

    while (i < count) {
      var b1 = BASE64_CHARS.indexOf(input[i])
      var b2 = (i + 1 < count) ? BASE64_CHARS.indexOf(input[i + 1]) : -1
      var c3 = (i + 2 < count) ? input[i + 2] : "="
      var c4 = (i + 3 < count) ? input[i + 3] : "="
      var b3 = c3 == "=" ? -1 : BASE64_CHARS.indexOf(c3)
      var b4 = c4 == "=" ? -1 : BASE64_CHARS.indexOf(c4)
      var last = i + 4 >= count
      i = i + 4

      if (b1 < 0 || b2 < 0) return null
      out.add(String.fromByte((b1 << 2) | (b2 >> 4)))

      // Padding may only appear in the final group, as "xx==" or "xxx=".
      if (b3 < 0) {
        if (b4 != -1 || !last) return null
      } else {
        out.add(String.fromByte(((b2 & 0xF) << 4) | (b3 >> 2)))
        if (b4 < 0) {
          if (!last) return null
        } else {
          out.add(String.fromByte(((b3 & 0x3) << 6) | b4))
        }
      }
    }

    return out.join("")
  }
}

class Config {
  static get(key) { `SELECT val FROM BIALET_CONFIG WHERE key = ?`.first([key])["val"] }
  static set(key, value) { `REPLACE INTO BIALET_CONFIG (key, val) VALUES (?, ?)`.query(key, value) }
  // Allow-list, as documented: true only for "1"/"true" (case-insensitive),
  // false for everything else including a missing key. The old denylist
  // (`!= "0"`) made "false", "off", "" and an absent row all true, so a
  // config-gated security decision failed open.
  static bool(key) {
    var v = get(key)
    if (v == null) return false
    var s = "%(v)".trim().lower
    return s == "1" || s == "true"
  }
  static num(key) { Num.fromString(get(key)) }
  static delete(key) { `DELETE FROM BIALET_CONFIG WHERE key = ?`.first(key) }
  static json(key) { Json.parse(get(key)) }
  static json(key, val) { set(key, Json.stringify(val)) }
  static enable(key) { set(key, "1") }
  static disable(key) { set(key, "0") }
}

class File {
  construct new(data) { set_(data) }
  construct get(id) { set_(`SELECT * FROM BIALET_FILES WHERE id = ? AND isTemp = 0`.first([id])) }
  construct create(name, type, file, size) { create_(name, type, file, size) }
  construct create(name, type, file) { create_(name, type, file, file.count) }
  create_(name, type, file, size) {
    var id = `INSERT INTO BIALET_FILES (name, originalFileName, type, file, size, isTemp) VALUES (?, ?, ?, ?, ?, 0)`.query([name, name, type, file, size])
    _name = name
    _id = id
    _type = type
    _size = size
    _file = file
    _isTemp = false
    _createdAt = null
  }
  set_(f) {
    if (!f) {
      _name = null
      _id = null
      _type = null
      _size = null
      _file = null
      _isTemp = false
      _createdAt = null
      return
    }
    _name = f["originalFileName"]
    _id = f["id"]
    _type = f["type"]
    _size = Num.fromString("%(f["size"])")
    _createdAt = f["createdAt"]
    // isTemp is 1 for a temporary row; the old comparison reported the
    // opposite of the row it described.
    _isTemp = f["isTemp"] == "1"
  }
  id { _id }
  type { _type }
  name { _name }
  size { _size }
  isTemp { _isTemp }
  createdAt { _createdAt }
  destroy { `DELETE FROM BIALET_FILES WHERE id = ?`.query(_id) }
  destroy() { destroy }
  save {
    _isTemp = false
    `UPDATE BIALET_FILES SET isTemp = 0 WHERE id = ?`.query(_id)
    return this
  }
  save() { save }
  temp {
    _isTemp = true
    `UPDATE BIALET_FILES SET isTemp = 1 WHERE id = ?`.query(_id)
    return this
  }
  temp() { temp }
}

class Db {
  static init {
    `CREATE TABLE IF NOT EXISTS BIALET_MIGRATIONS (version TEXT, createdAt DATETIME DEFAULT CURRENT_TIMESTAMP)`.query()
    `CREATE TABLE IF NOT EXISTS BIALET_SESSION (id TEXT, key TEXT, val TEXT, updatedAt DATETIME, PRIMARY KEY (id, key))`.query()
    // Older versions created BIALET_SESSION without a primary key, so REPLACE
    // inserted duplicate rows and reads were nondeterministic. Rebuild the table
    // in place when that schema is detected.
    if (`SELECT COUNT(*) FROM pragma_table_info('BIALET_SESSION') WHERE pk > 0`.toNum == 0) {
      `ALTER TABLE BIALET_SESSION RENAME TO BIALET_SESSION_OLD`.query()
      `CREATE TABLE BIALET_SESSION (id TEXT, key TEXT, val TEXT, updatedAt DATETIME, PRIMARY KEY (id, key))`.query()
      `INSERT OR IGNORE INTO BIALET_SESSION (id, key, val, updatedAt) SELECT id, key, val, updatedAt FROM BIALET_SESSION_OLD ORDER BY updatedAt DESC`.query()
      `DROP TABLE BIALET_SESSION_OLD`.query()
    }
    `CREATE TABLE IF NOT EXISTS BIALET_CONFIG (key TEXT PRIMARY KEY, val TEXT)`.query()
    `CREATE TABLE IF NOT EXISTS BIALET_LOGS (message TEXT, createdAt DATETIME DEFAULT CURRENT_TIMESTAMP)`.query()
    `CREATE TABLE IF NOT EXISTS BIALET_FILES (id INTEGER PRIMARY KEY, name TEXT, originalFileName TEXT, type TEXT, size INTEGER, file BLOB, isTemp INTEGER DEFAULT 1, createdAt DATETIME DEFAULT CURRENT_TIMESTAMP)`.query()
    // The upload path reads SUM(size) and COUNT(*) WHERE isTemp = 1 before the
    // handler runs, so without this index every multipart request scans every
    // file row, including the app's own saved files.
    `CREATE INDEX IF NOT EXISTS BIALET_FILES_ISTEMP ON BIALET_FILES (isTemp)`.query()
    // Session.set's write-path sweep filters on updatedAt; without this index
    // both the sweep and Session.destroy scan every row.
    `CREATE INDEX IF NOT EXISTS BIALET_SESSION_UPDATED ON BIALET_SESSION (updatedAt)`.query()
    `CREATE TABLE IF NOT EXISTS BIALET_REMOTE_MODULES (module TEXT PRIMARY KEY, content TEXT, createdAt DATETIME DEFAULT CURRENT_TIMESTAMP)`.query()
    // Run again the initiliziation once we know the table exists
    // Missing BIALET_TIMEZONE yields null; Date.tz must stay a string (the
    // primitives reject a non-string with a catchable error).
    Date.init(Config.get("BIALET_TIMEZONE") || "")
  }

  static clean {
    // TODO: Set expiration session and files date in config
    `DELETE FROM BIALET_SESSION WHERE updatedAt < date('now', '-1 year')`.query()
    `DELETE FROM BIALET_FILES WHERE isTemp = 1 AND createdAt < date('now', '-1 day')`.query()
  }

  static migrate(version, schemaOrCallback) {
    // Db.init runs Date.init before this point, so a callback may call
    // Date.new() and do date arithmetic. Migrations never run before the
    // date subsystem is initialized.
    Db.init
    if (!`SELECT version FROM BIALET_MIGRATIONS WHERE version = ?`.first([version])) {
      if (schemaOrCallback is Fn) {
        schemaOrCallback.call()
      } else {
        schemaOrCallback.toString.split(";").each{|q| Query.fromString(q, []) }
      }
      `INSERT INTO BIALET_MIGRATIONS (version) VALUES (?)`.query([version])
      System.print("Migration applied - %(version)")
    }
    Db.clean
  }
  static save(table, values) { Query.new(table).save(values) }
  static delete(table, id) {
    // Same contract as save(): the table is interpolated into SQL, so it must
    // be a plain identifier and it must not name a framework table.
    var name = Query.identifier_(table, "table name")
    if (name.upper.startsWith("BIALET_")) return false
    return Query.fromString("DELETE FROM `%(name)` WHERE id = ?", [id])
  }
}

class Http {
  construct new() {
    _method = false
    _status = 0
    _headers = {}
    _body = ""
    _error = 0
    _errorMessage = ""
    _fullHeaders = ""
    _postData = ""
  }
  body { _body }
  status { _status }
  headers { _headers }
  headers(name) { _headers.containsKey(name) ? _headers[name] : null }
  error { _error }
  errorMessage { _errorMessage }
  method { _method }
  method=(m) { _method = m }
  postData=(data) {
    if (!_method) {
      _method = "POST"
    }
    _postData = data ? (data is String ? data : Json.stringify(data)) : ""
  }
  // An outbound request is assembled as raw header text, so a header *value*
  // carrying CRLF would append headers of the attacker's choosing to the
  // request Bialet sends (e.g. "X-Forwarded-For: 127.0.0.1", or a second Host:
  // when the value is forwarded from user input). Reject control characters,
  // blank names and names containing ':' exactly like a response header, and
  // validate the method so it cannot rewrite the request line either.
  static safeMethod(method) {
    var value = method == null ? "GET" : "%(method)".trim()
    if (value.count == 0 || value.count > 32) {
      Fiber.abort("Invalid HTTP method")
    }
    for (char in value) {
      var byte = char.bytes[0]
      if (!((byte >= 65 && byte <= 90) || (byte >= 97 && byte <= 122))) {
        Fiber.abort("Invalid HTTP method")
      }
    }
    return value.upper
  }

  static safeUrl(url) {
    var value = url == null ? "" : "%(url)"
    for (char in value) {
      var byte = char.bytes[0]
      if (byte <= 32 || byte == 127) {
        Fiber.abort("Invalid URL: control characters are not allowed")
      }
    }
    return value
  }

  call(url, options) {
    if (!_method) {
      _method = "GET"
    }
    // Build this call's header set in a local map: the caller's options are an
    // input, and writing the token's Authorization (or the default Content-Type)
    // into a map it reuses would send that credential to the next origin.
    var headers = {}
    var given = options["headers"]
    if (given != null) {
      for (h in given) headers[h.key] = h.value
    }
    // Per-call options live in locals, not on the instance: an Http object
    // reused for another destination must not inherit the previous call's
    // credentials or body.
    var postData = options.containsKey("form") ? Util.params(options["form"]) : _postData
    var basicAuth = ""
    if (options.containsKey("form")) {
      headers["Content-Type"] = "application/x-www-form-urlencoded"
    } else if (!headers.containsKey("Content-Type")) {
      headers["Content-Type"] = "application/json"
    }
    if (options["token"] != null) {
      headers["Authorization"] = "Bearer %(options["token"])"
    }
    if (options["basicAuth"] != null) {
      basicAuth = options["basicAuth"]["username"] + ":" + options["basicAuth"]["password"]
    }
    var timeout = options.containsKey("timeout") ? options["timeout"] : 0
    var connectTimeout = options.containsKey("connectTimeout") ? options["connectTimeout"] : 0
    // The option is documented as the whole call's transfer timeout. A redirect
    // chain is one call, so it gets one deadline; re-arming the timeout on every
    // hop let one call hold the only worker for eleven times the budget.
    var budget = timeout > 0 ? timeout : 20000
    var deadline = Util.monotonicMs + budget
    var method = Http.safeMethod(_method)
    var current = Http.safeUrl(url)

    // Follow redirects here, one transfer per hop, instead of in the transport:
    // the transport merges every hop's header block into one buffer, so a
    // Set-Cookie from the redirect target was stored under the host of the
    // original URL. One transfer per hop keeps each cookie with the host that
    // emitted it, and lets credentials stop at the origin that issued them.
    var hops = 0
    var sendCredentials = true
    while (true) {
      var hopTimeout = deadline - Util.monotonicMs
      if (hopTimeout <= 0) {
        // The call, not a hop, expired.
        _error = 28
        _errorMessage = "Operation timed out"
        _status = 0
        _fullHeaders = ""
        _body = ""
        break
      }
      // The native layer truncates a double to whole milliseconds and reads 0 as
      // "no timeout given"; never let a live remainder become that sentinel.
      if (hopTimeout < 1) hopTimeout = 1
      var hopHeaders = Http.headerBlock_(headers, current, sendCredentials)
      var response = call_(current, method, hopHeaders, postData, basicAuth,
                           hopTimeout, connectTimeout)
      _error = response[3]
      _errorMessage = response[4]
      var rawHeaders = response[1] ? response[1].trim() : ""
      if (_error == 0) {
        Http.storeCookies(rawHeaders, current)
      }
      var status = response[0]
      var location = Http.locationOf_(rawHeaders)
      if (_error != 0 || status < 300 || status >= 400 || location == null || hops >= 10) {
        _status = status
        _fullHeaders = rawHeaders
        _body = response[2] ? response[2].trim() : ""
        break
      }
      var next = Http.resolveUrl_(current, location)
      if (!Http.sameOrigin_(current, next)) {
        // Credentials, explicit cookies and basic auth belong to one origin.
        // Stop emitting them from here on; do not mutate the caller's map, and
        // do not test exact spellings (header names are case-insensitive).
        sendCredentials = false
        // The native call expects a string here, not null.
        basicAuth = ""
      }
      // curl's default: 303, and a POST answered by 301/302, continue as GET.
      if (status == 303 || ((status == 301 || status == 302) && method == "POST")) {
        method = "GET"
        postData = ""
      }
      // Every hop is a URL the transport will dial, so it passes the same gate
      // as the caller's URL: a Location is attacker-controlled text.
      current = Http.safeUrl(next)
      hops = hops + 1
    }

    _headers = {}
    if (_fullHeaders) {
      var tmp
      var headerName
      var headerValue
      for (line in _fullHeaders.split("\n")) {
        tmp = line.split(":")
        headerName = tmp.removeAt(0).trim().lower
        headerValue = tmp.join(":").trim().lower
        _headers[headerName] = headerValue
      }
    }

    return _error == 0
  }

  // Header block for one transfer: caller headers plus the cookies the jar holds
  // for exactly this host. When sendCredentials is false (the redirect crossed
  // an origin), Authorization and Cookie are dropped by name, not by exact map
  // key, so every spelling stops at the origin that issued it.
  static headerBlock_(headers, url, sendCredentials) {
    var lines = []
    var hasCookie = false
    for (h in headers) {
      var name = Util.headerName(h.key)
      if (name.lower == "cookie") hasCookie = true
      if (!sendCredentials && Http.isCredentialHeader_(name)) continue
      lines.add("%(name): %(Util.headerValue(h.value))")
    }
    if (!hasCookie) {
      var jar = Http.jarFor(url)
      if (jar != "") {
        lines.add("Cookie: %(Util.headerValue(jar))")
      }
    }
    return lines.join("\n")
  }

  static isCredentialHeader_(name) {
    var n = "%(name)".trim().lower
    return n == "authorization" || n == "cookie"
  }

  // Last Location header of a response block, or null when there is none.
  static locationOf_(rawHeaders) {
    var found = null
    for (line in rawHeaders.split("\n")) {
      var sep = line.indexOf(":")
      if (sep <= 0) continue
      if (line[0...sep].trim().lower != "location") continue
      found = line[sep + 1...line.count].trim()
    }
    return found
  }

  // Resolve a Location value against the URL that produced it.
  static resolveUrl_(base, location) {
    var loc = location.trim()
    if (loc == "") return base
    var lower = loc.lower
    if (lower.startsWith("http://") || lower.startsWith("https://")) {
      // A redirect never needs credentials in its authority; accepting one
      // would let this parser and libcurl disagree about the host.
      if (Http.requestAuthority_(loc).contains("@")) {
        Fiber.abort("Invalid redirect: credentials in Location")
      }
      var hash = loc.indexOf("#")
      return hash >= 0 ? loc[0...hash] : loc
    }
    var scope = urlScope_(base)
    var origin = "%(scope["scheme"])://%(scope["authority"])"
    if (loc.startsWith("//")) {
      if (Http.requestAuthority_(loc).contains("@")) {
        Fiber.abort("Invalid redirect: credentials in Location")
      }
      var hash = loc.indexOf("#")
      return "%(scope["scheme"]):" + (hash >= 0 ? loc[0...hash] : loc)
    }
    var path = scope["path"]
    var dir = "/"
    for (i in 0...path.count) {
      if (path[i] == "/") dir = path[0...i + 1]
    }
    var hash = loc.indexOf("#")
    if (hash >= 0) loc = loc[0...hash]
    if (loc.startsWith("/")) return origin + loc
    return origin + Http.normalizePath_(dir + loc)
  }

  // Collapse "." and ".." segments so a relative Location cannot walk above
  // the origin root.
  static normalizePath_(path) {
    var out = []
    for (seg in path.split("/")) {
      if (seg == "" || seg == ".") {
        if (seg == "" && out.count == 0) out.add("")
        continue
      }
      if (seg == "..") {
        if (out.count > 1) out.removeAt(out.count - 1)
        continue
      }
      out.add(seg)
    }
    var joined = out.join("/")
    if (!joined.startsWith("/")) joined = "/" + joined
    return joined
  }

  static sameOrigin_(a, b) {
    var sa = urlScope_(a)
    var sb = urlScope_(b)
    return sa["scheme"] == sb["scheme"] && sa["host"] == sb["host"] &&
           sa["port"] == sb["port"]
  }

  static jar {
    if (!__cookieJar || __cookieJar.count == 0) return ""
    var all = []
    for (jarHost in __cookieJar) {
      for (entry in jarHost.value) {
        all.add("%(entry.key)=%(entry.value["v"])")
      }
    }
    return all.join("; ")
  }
  static jarFor(url) {
    if (!__cookieJar || __cookieJar.count == 0) return ""
    var scope = urlScope_(url)
    var host = scope["host"]
    var path = scope["path"]
    var secure = scope["scheme"] == "https"
    var now = Date.tz != null ? Date.now.unix : 0
    var matched = []
    for (jarHost in __cookieJar) {
      // Host-only jar: every cookie is keyed on the host that set it, so the
      // key matches its own host exactly and nothing else.
      if (host != jarHost.key) continue
      for (entry in jarHost.value) {
        var c = entry.value
        if (c["s"] && !secure) continue
        var cp = c["p"]
        // RFC 6265 path-match: exact, or a prefix ending on a '/' boundary.
        var pathMatches = path == cp || (path.startsWith(cp) && (cp.endsWith("/") || path[cp.count] == "/"))
        if (!pathMatches) continue
        if (c["e"] > 0 && now >= c["e"]) continue
        matched.add("%(entry.key)=%(c["v"])")
      }
    }
    return matched.join("; ")
  }
  // Host-only jar: the Set-Cookie Domain attribute is ignored entirely. It
  // cannot be honoured safely without an embedded public-suffix list -- a
  // response from evil.co.uk carrying "Domain=co.uk" would scope the cookie to
  // every co.uk registrant, including the victim.co.uk the app calls next.
  // Keying every cookie on the responding host keeps the documented invariant
  // ("a cookie set by one host is never sent to a different host") and removes
  // the multi-label suffix path without shipping a suffix list.
  static storeCookies(rawHeaders, url) {
    if (rawHeaders == "") return
    var scope = urlScope_(url)
    var reqHost = scope["host"]
    if (!__cookieJar) __cookieJar = {}
    for (line in rawHeaders.split("\n")) {
      var sep = line.indexOf(":")
      if (sep <= 0) continue
      var headerName = line[0...sep].trim().lower
      if (headerName != "set-cookie") continue
      var value = line[sep + 1...line.count].trim()
      var parts = value.split(";")
      var kv = parts[0].split("=")
      if (kv.count < 2) continue
      var cookieName = kv[0].trim()
      var cookie = {"v": kv[1].trim(), "p": "/", "s": false, "e": 0}
      for (i in 1...parts.count) {
        var attr = parts[i].trim()
        var eq = attr.indexOf("=")
        var attrName = (eq >= 0 ? attr[0...eq] : attr).trim().lower
        var attrValue = eq >= 0 ? attr[eq + 1...attr.count].trim() : ""
        if (attrName == "domain") {
          // Deliberately ignored: see the note above storeCookies. The cookie
          // stays keyed on the responder instead of widening to a suffix.
        } else if (attrName == "path" && attrValue != "") {
          cookie["p"] = attrValue
        } else if (attrName == "secure") {
          cookie["s"] = true
        } else if (attrName == "max-age") {
          var secs = Num.fromString(attrValue)
          if (secs && Date.tz != null) cookie["e"] = Date.now.unix + secs
        }
      }
      if (!__cookieJar.containsKey(reqHost)) __cookieJar[reqHost] = {}
      __cookieJar[reqHost][cookieName] = cookie
    }
  }
  static urlScope_(url) {
    url = "%(url)"
    var scheme = "http"
    var rest = url
    var schemeEnd = url.indexOf("://")
    if (schemeEnd > 0) {
      scheme = url[0...schemeEnd].lower
      rest = url[schemeEnd + 3...url.count]
    }
    // RFC 3986: the fragment is not part of the authority, path or query, and
    // the authority ends at the first '/', '?' or '#'. libcurl parses the URL
    // the same way, so an origin comparison built on this scope agrees with the
    // host the transport will actually contact.
    var hash = rest.indexOf("#")
    if (hash >= 0) rest = rest[0...hash]
    var end = rest.count
    for (i in 0...rest.count) {
      var c = rest[i]
      if (c == "/" || c == "?") {
        end = i
        break
      }
    }
    var authority = rest[0...end]
    var path = end < rest.count ? rest[end...rest.count] : "/"
    if (path[0] == "?") path = "/" + path
    var queryStart = path.indexOf("?")
    if (queryStart >= 0) path = path[0...queryStart]
    // A URL may carry userinfo; the transport connects to the host after the
    // last '@', so the jar key and the origin must not include it (String has no
    // lastIndexOf, so walk forward).
    var at = authority.indexOf("@")
    while (at >= 0) {
      authority = authority[at + 1...authority.count]
      at = authority.indexOf("@")
    }
    authority = authority.lower
    var host = authority
    var port = ""
    if (host.startsWith("[")) {
      var close = host.indexOf("]")
      if (close >= 0) {
        if (close + 1 < authority.count && authority[close + 1] == ":") {
          port = authority[close + 2...authority.count]
        }
        host = authority[0...close + 1]
      }
    } else {
      var colon = host.indexOf(":")
      if (colon >= 0) {
        port = host[colon + 1...host.count]
        host = host[0...colon]
      }
    }
    if (port == "") port = scheme == "https" ? "443" : "80"
    return {"scheme": scheme, "host": host, "port": port,
            "authority": "%(host):%(port)", "path": path}
  }

  // Authority text of an absolute or scheme-relative URL, used to reject a
  // redirect whose credentials would make this parser and libcurl disagree
  // about the host.
  static requestAuthority_(absolute) {
    var rest = absolute
    var schemeEnd = absolute.indexOf("://")
    if (schemeEnd > 0) rest = absolute[schemeEnd + 3...absolute.count]
    if (rest.startsWith("//")) rest = rest[2...rest.count]
    var end = rest.count
    for (i in 0...rest.count) {
      var c = rest[i]
      if (c == "/" || c == "?" || c == "#") {
        end = i
        break
      }
    }
    return rest[0...end]
  }
  static query(params) { Util.params(params) }
  static url(base, params) {
    var qs = Util.params(params)
    if (qs == "") return base
    if (base.contains("?")) return base + "&" + qs
    return base + "?" + qs
  }

  // Shortcuts (Http.get/post/put/delete) never let a request take the page
  // down: transport failures, non-2xx statuses, and anything unexpected
  // while reading the response (e.g. malformed JSON) all just return null.
  // Check Http.error / Http.errorMessage / Http.status for the last call's
  // outcome instead of wrapping every call site in its own Fiber.
  static request(url, method, data, options) {
    __http = Http.new()
    __http.method = method
    __http.postData = data
    var fiber = Fiber.new {
      if (!__http.call(url, options)) return null
      if (__http.status < 200 || __http.status >= 300) return null
      var contentType = __http.headers("content-type") || ""
      if (contentType.contains("text/json") || contentType.contains("application/json")) {
        return Json.parse(__http.body)
      }
      return __http.body
    }
    var value = fiber.try()
    __error = fiber.error ? 1 : __http.error
    __errorMessage = fiber.error ? fiber.error.toString : __http.errorMessage
    return fiber.error ? null : value
  }
  static error { __error || 0 }
  static errorMessage { __errorMessage || "" }
  static status { __http ? __http.status : 0 }
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

class Cron {
  static run_(should, job) {
    if (should) {
      var res = job.call(__now)
      System.print("Running cron: %(res)")
    }
    return should
  }
  static at(hours, minutes, dayOfWeek, job) {
    if (!__now) __now = Date.now
    return run_(__now.hours == hours && __now.minutes == minutes && __now.weekday == dayOfWeek, job)
  }
  static at(hours, minutes, job) {
    if (!__now) __now = Date.now
    return run_(__now.hours == hours && __now.minutes == minutes, job)
  }
  static every(minutes, job) {
    if (!__now) __now = Date.now
    return run_(__now.minutes % minutes == 0, job)
  }
}

class Markdown {
  static html(markdown) { html_(markdown).raw }
  static file(name) {
    var content = file_(name)
    return content is String ? content.raw : content
  }
}
