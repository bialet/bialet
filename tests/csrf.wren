var sess = Session.new()

if (Request.isPost) {
  return sess.csrfOk ? "ok" : "fail"
} else {
  return sess.csrf
}
