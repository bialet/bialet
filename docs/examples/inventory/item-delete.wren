import "_app" for Auth

if (!Auth.user) return Response.redirect("/login")
if (!Request.isPost) return Response.redirect("/items")

var session = Session.new()
if (session.csrfOk) {
  var id = Request.post("id") || ""
  `DELETE FROM items WHERE id = ?`.query(id)
}
return Response.redirect("/items")
