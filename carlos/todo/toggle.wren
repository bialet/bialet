import "_app/domain" for Task

// POST-only toggle handler. Fails closed on a bad CSRF token:
// no state change, still redirect back so the user isn't staring at a 500.
var session = Session.new()

if (Request.isPost && session.csrfOk) {
  var task = Task.find(Request.post("id") || "0")
  if (task) task.toggle()
}

return Response.redirect("/")
