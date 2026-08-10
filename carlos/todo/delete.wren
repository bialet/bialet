import "_app/domain" for Task

// POST-only delete handler. Same fail-closed CSRF pattern as toggle.wren.
var session = Session.new()

if (Request.isPost && session.csrfOk) {
  Task.delete(Request.post("id") || "0")
}

return Response.redirect("/")
