class Response {
  static init() {
    __headers = {"Content-Type": "text/html"}
    __status = 200
    __out = ""
  }
  static out(out) { __out = __out + "\r\n" + out }
  static out() { __out.trim() }
  static status(status) { __status = status }
  static status() { __status }
  static headers() { __headers.keys.map{|k| k + ": " + __headers[k] + "\r\n"}.join() }
  static header(header, value) { __headers[header.trim()] = value.trim() }
  static redirect(url) {
    Response.status(302)
    Response.header("Location", url)
  }
}

class Util {

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
    for (line in lines) {
      if (line.trim() == "") {
        startBody = true
        continue
      }
      if (!startBody) {
        tmp = line.split(":")
        __headers[tmp.removeAt(0).trim()] = tmp.join(":").trim()
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
  static set(name, value, options) {
      // TODO Add multiple cookies!
    Request.header("Set-Cookie", name + "=" + value + "; " + options.map{|k, v| k + "=" + v}.join("; "))
  }
  static set(name, value){ set(name, value, {}) }
}

// TODO: Sessions
class Session {
}

class Db {
  foreign static intQuery(query, params)
  foreign static intLastInsertId()
  static query(query, params){
    var res = Db.intQuery(query, params)
    if (res is Num) {
      return res > 0
    }
    if (!res is List) {
      res = []
    }
    return res
  }
  static lastInsertId(){ intLastInsertId() }
  static migrate(version, schema) {
    Db.query("CREATE TABLE IF NOT EXISTS BIALET_MIGRATIONS (version TEXT, createdAt DATETIME DEFAULT CURRENT_TIMESTAMP)")
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

Response.init()
