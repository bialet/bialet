import "_domain" for Task

var id = Request.post("id") || ""
Task.delete(id)
Response.redirect("/")
