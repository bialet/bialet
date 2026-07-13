var sess = Session.new()

if (Request.get("set")) {
  sess.set("user", "testuser")
  return "set"
} else if (Request.get("get")) {
  var user = sess.get("user")
  return user ? user : "empty"
} else {
  return "ready"
}
