// toggle.wren — POST-only. Marks a task done/undone, then redirects back.
// The docs example omitted CSRF here (oops, that was the "minimum" version),
// so I'm adding it — the task brief said CSRF on EVERY state-changing form.

import "_app/domain" for Task

var session = Session.new()

if (!Request.isPost) {
  return Response.redirect("/")
}

if (!session.csrfOk) {
  Response.status(400)
  return "Bad CSRF token"
}

Task.new({ "id": Request.post("id") || "" }).toggle()
return Response.redirect("/")
