import "_domain" for Task

var id = Request.post("id")
if (id) Task.toggle(id)
Response.redirect("/")
