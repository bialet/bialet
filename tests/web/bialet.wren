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
  static header(header, value) {
    __headers[header] = value
  }
}

Response.init()
