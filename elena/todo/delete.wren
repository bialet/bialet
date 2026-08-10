// delete.wren — POST-only. Deletes a task, then redirects back. Same CSRF dance.

import "_app/domain" for Task

var session = Session.new()

if (!Request.isPost) {
  return Response.redirect("/")
}

if (!session.csrfOk) {
  Response.status(400)
  return "Bad CSRF token"
}

Task.delete(Request.post("id") || "")
return Response.redirect("/")
