import "_app" for Auth

var user = Auth.user
return Response.redirect(user ? "/items" : "/login")
