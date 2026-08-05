// Toggle endpoint. A tiny "controller" file, one job, like a PHP
// endpoint script. POST-only, CSRF-checked, redirect back.
import "_app/domain" for Task

var session = Session.new()

if (Request.isPost && session.csrfOk) {
  var id = Request.post("id") || ""
  if (id != "") Task.toggle(id)
}
return Response.redirect("/")
