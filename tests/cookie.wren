if (Request.get("set")) {
  Cookie.set("test_cookie", "cookie_value")
  Response.out("set")
} else {
  var value = Cookie.get("test_cookie")
  Response.out(value ? value : "empty")
}
