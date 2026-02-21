var sess = Session.new()

if (Request.get("set")) {
  sess.set("user", "testuser")
  Response.out("set")
} else if (Request.get("get")) {
  var user = sess.get("user")
  Response.out(user ? user : "empty")
} else {
  Response.out("ready")
}
