if (Request.get("set")) {
  Cookie.set("test_cookie", "cookie_value")
  return "set"
} else {
  var value = Cookie.get("test_cookie")
  return value ? value : "empty"
}
