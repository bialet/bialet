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

Response.init()

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
    // TODO URL decode
    query.split("&").each{|q| all[q.split("=")[0]] = q.split("=")[1]}
    return all
  }
  static header(name) { __headers[name] ? __headers[name]:"" }
  static get(name) { __get[name] ? __get[name]:"" }
  static post(name) { __post[name] ? __post[name]:"" }
  static method() { __method }
  static uri() { __uri }
  static body() { __body }
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
  foreign static query(query, params)
  foreign static lastInsertId()
  static migrate(version, schema) {
    if (!Db.one("SELECT version FROM _BIALET_MIGRATIONS WHERE version = ?", version)) {
      if (Db.query(schema)) {
        Db.query("INSERT INTO _BIALET_MIGRATIONS (version) VALUES (?)", [version])
      }
    }
  }
  static query(query) { Db.query(query, []) }
  static all(query) { Db.query(query, []) }
  static all(query, params) { Db.query(query, params) }
  static one(query, params) { Db.all(query + " limit 1", params)[0] }
  static one(query) { Db.one(query, []) }
  static save(table, values) {
    Db.all("REPLACE INTO " + table + "(" + values.keys.join(", ") + ") VALUES (" + values.map{|v| "?"}.join(", ") + ")", values.values.toList)
    return Db.lastInsertId()
  }
}
