// Toggle endpoint. The "action" file for the per-row button.
import "_app/domain" for Task

var session = Session.new()

if (Request.isPost && session.csrfOk) {
  var id = Request.post("id") || ""
  if (id != "") Task.toggle(id)
}
return Response.redirect("/")
