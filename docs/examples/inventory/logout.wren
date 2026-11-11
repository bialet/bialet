import "_app" for Auth

if (!Auth.user) return Response.redirect("/login")
if (!Request.isPost) return Response.redirect("/items")

var session = Session.new()
if (session.csrfOk) Session.destroy()
return Response.redirect("/login")
