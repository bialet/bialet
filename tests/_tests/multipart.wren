var boundary = "XyZ-boundary"

var field = Fn.new {|name, value|
  return "--%(boundary)\r\nContent-Disposition: form-data; name=\"%(name)\"\r\n\r\n%(value)\r\n"
}
var file = Fn.new {|name, filename, content|
  return "--%(boundary)\r\nContent-Disposition: form-data; name=\"%(name)\"; filename=\"%(filename)\"\r\nContent-Type: text/csv\r\n\r\n%(content)\r\n"
}
var finish = "--%(boundary)--\r\n"

var send = Fn.new {|body|
  return Test.post("/multipart", body).setHeader("Content-Type", "multipart/form-data; boundary=%(boundary)")
}

// A text field and a file in the same form
send.call(field.call("a", "hola") + file.call("excel", "x.csv", "hi") + finish)
  .status(200)
  .equals("a=hola|b=null|c=null|note=null|title=null|excel=null|up=null|empty=null|file=x.csv:2")

// Text only, no file
send.call(field.call("a", "hola") + finish)
  .status(200)
  .equals("a=hola|b=null|c=null|note=null|title=null|excel=null|up=null|empty=null|file=none")

// A newline in the value is kept, with CRLF normalized to LF
send.call(field.call("note", "uno\r\ndos\ntres") + finish)
  .status(200)
  .contains("note=uno\ndos\ntres|")

// UTF-8, '=' and '&' in the value
send.call(field.call("a", "camión & ñandú = 100\%") + field.call("b", "a=b&c=d+e") + finish)
  .status(200)
  .contains("a=camión & ñandú = 100\%|b=a=b&c=d+e|")

// A value that looks like part of the multipart framing
send.call(field.call("a", "--%(boundary)x\r\nContent-Disposition: form-data; name=\"b\"") + finish)
  .status(200)
  .contains("b=null|")

// An empty value is an empty string, not null
send.call(field.call("empty", "") + finish)
  .status(200)
  .contains("empty=|file=")

// Fields before and after the file are all read
send.call(
  field.call("a", "first") + field.call("b", "second") +
  file.call("excel", "x.csv", "data") +
  field.call("c", "third") + field.call("title", "last") + finish)
  .status(200)
  .equals("a=first|b=second|c=third|note=null|title=last|excel=null|up=null|empty=null|file=x.csv:4")

// A file never shows up in Request.post
send.call(file.call("excel", "x.csv", "secret-content") + finish)
  .status(200)
  .contains("excel=null|")
  .notContains("secret-content")

// A file input left blank by the browser is neither a field nor a file
send.call(field.call("a", "hola") + file.call("up", "", "") + finish)
  .status(200)
  .contains("a=hola|")
  .contains("up=null|")
  .contains("file=none")

// The last value of a repeated field wins, like urlencoded forms
send.call(field.call("a", "one") + field.call("a", "two") + finish)
  .status(200)
  .contains("a=two|")

// Escaped quotes in the field name, and the name before or after filename
send.call(field.call("a\\\"b", "x") + finish)
  .status(200)
  .contains("a=null|")
send.call("--%(boundary)\r\nContent-Disposition: form-data; filename=\"x.csv\"; name=\"excel\"\r\n\r\nzz\r\n" + finish)
  .status(200)
  .contains("file=x.csv:2")

// A file part without a name stays unnamed: it must not borrow the value of
// another parameter (here, its own filename)
Test.post("/multipart?unnamed=1", "--%(boundary)\r\nContent-Disposition: form-data; filename=\"leak.csv\"\r\n\r\nzz\r\n" + finish)
  .setHeader("Content-Type", "multipart/form-data; boundary=%(boundary)")
  .status(200)
  .equals("unnamed")

// Unquoted parameter values
send.call("--%(boundary)\r\nContent-Disposition: form-data; name=plain\r\n\r\nvalue\r\n" + finish)
  .status(200)
  .contains("a=null|")

// A part with no blank line after its headers is skipped without swallowing
// the parts that follow it
send.call("--%(boundary)\r\nContent-Disposition: form-data; name=\"a\"\r\n" + field.call("b", "bee") + finish)
  .status(200)
  .contains("a=null|b=bee|")

// A part with no Content-Disposition is skipped
send.call("--%(boundary)\r\nContent-Type: text/plain\r\n\r\nzz\r\n" + field.call("a", "kept") + finish)
  .status(200)
  .contains("a=kept|")

// A boundary longer than the buffer is rejected, not truncated
var longBoundary = "B" * 300
Test.post("/multipart", "--%(longBoundary)\r\nContent-Disposition: form-data; name=\"a\"\r\n\r\nx\r\n--%(longBoundary)--\r\n")
  .setHeader("Content-Type", "multipart/form-data; boundary=%(longBoundary)")
  .status(200)
  .contains("a=null|")

// The boundary may be quoted in the Content-Type
Test.post("/multipart", field.call("a", "quoted") + finish)
  .setHeader("Content-Type", "multipart/form-data; boundary=\"%(boundary)\"")
  .status(200)
  .contains("a=quoted|")

// Parameters after the boundary do not leak into it
Test.post("/multipart", field.call("a", "param") + finish)
  .setHeader("Content-Type", "multipart/form-data; boundary=%(boundary); charset=utf-8")
  .status(200)
  .contains("a=param|")

// CSRF token in a multipart field
`REPLACE INTO BIALET_SESSION (id, key, val, updatedAt) VALUES (?, ?, ?, CURRENT_TIMESTAMP)`.query(["mp-sess", "_bialet_csrf", "mp-token-123"])
Test.post("/csrf", field.call("_bialet_csrf", "mp-token-123") + file.call("excel", "x.csv", "hi") + finish)
  .setHeader("Content-Type", "multipart/form-data; boundary=%(boundary)")
  .setHeader("Cookie", "BIALETSESSID=mp-sess")
  .status(200)
  .equals("ok")
Test.post("/csrf", field.call("_bialet_csrf", "wrong") + finish)
  .setHeader("Content-Type", "multipart/form-data; boundary=%(boundary)")
  .setHeader("Cookie", "BIALETSESSID=mp-sess")
  .status(200)
  .equals("fail")

// Regression: urlencoded and JSON still work
Test.post("/multipart", {"a": "url", "b": "enc"})
  .status(200)
  .contains("a=url|b=enc|")
Test.post("/request-meta", {"foo": "bar"})
  .status(200)
  .equals("form|/request-meta|foo=bar|application/x-www-form-urlencoded")
Test.apiPost("/request-meta", {"key": "jsonvalue"})
  .status(200)
  .equals("json|/request-meta|jsonvalue")
