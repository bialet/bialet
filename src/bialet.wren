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
}

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
}

class Html {
  static escape(str) {
    if (!str) return ""
    return str.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\"", "&quot;")
  }
}

class Request {
  static init(message) {
    __message = message
    __headers = {}
    __get = {}
    __post = {}
    var lines = message.split("\n")
    var tmp = lines.removeAt(0).split(" ")
    __method = tmp[0]
    __fullUri = tmp[1]
    __body = ""
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
        if (headerName == "Cookie") {
          Cookie.init(headerValue)
        }
        __headers[headerName] = headerValue
      } else {
        __body = __body + line
      }
    }
    if (__method == "POST") {
      __post = parseQuery(__body)
    }
  }
  static parseQuery(query) {
    var all = {}
    query = query.trim()
    if (query == "") {
      return all
    }
    query.split("&").each{|q|
      var value = ""
      var tmp = q.split("=")
      var key = Util.urlDecode(tmp[0])
      if (tmp.count > 0) {
        value = Util.urlDecode(tmp[1])
      }
      all[key] = value
    }
    return all
  }
  static header(name) { __headers[name] ? __headers[name]:"" }
  static get(name) { __get[name] ? __get[name]:"" }
  static post(name) { __post[name] ? __post[name]:"" }
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
    Response.addCookieHeader("%(name)=%(value); %( options.map{|k, v| "%(k)=%(v)"}.join("; ") )")
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
  construct new() {
    _id = Cookie.get(Session.name)
    if (!_id) {
      _id = Util.randomString(40)
      Cookie.set(Session.name, _id)
    }
    __values = {}
    var res = Db.all("SELECT key, val FROM BIALET_SESSION WHERE id = ?", [_id])
    if (res && res.count > 0) {
      System.print(res)
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
  foreign static intQuery(query, params)
  foreign static intLastInsertId()
  static query(query, params){
    var res = Db.intQuery(query, params)
    if (res is List) {
      return res
    } else {
      System.write("Res is not List (%( res.type ))")
      return []
    }
  }
  static lastInsertId(){ intLastInsertId() }
  static migrate(version, schema) {
    Db.query("CREATE TABLE IF NOT EXISTS BIALET_MIGRATIONS (version TEXT, createdAt DATETIME DEFAULT CURRENT_TIMESTAMP)")
    Db.query("CREATE TABLE IF NOT EXISTS BIALET_SESSION (id TEXT, key TEXT, val TEXT, updatedAt DATETIME)")
    if (!Db.one("SELECT version FROM BIALET_MIGRATIONS WHERE version = ?", [version])) {
      if (Db.query(schema)) {
        Db.query("INSERT INTO BIALET_MIGRATIONS (version) VALUES (?)", [version])
      }
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

Cookie.init()
Response.init()
