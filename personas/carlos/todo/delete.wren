// Delete endpoint. POST-only, CSRF-checked, redirect back.
import "_app/domain" for Task

var session = Session.new()

if (Request.isPost && session.csrfOk) {
  var id = Request.post("id") || ""
  if (id != "") Task.remove(id)
}
return Response.redirect("/")
