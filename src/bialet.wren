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
}

class Request {
  foreign static url()
  foreign static headers()
  foreign static get(key)
  foreign static post(key)
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
    Db.all("REPLACE INTO " + table + "(" + values.keys.join(", ") + ") VALUES (" + values.map{|v| "? "}.join() + ")", values)
    return Db.lastInsertId()
  }
}

Response.init()
