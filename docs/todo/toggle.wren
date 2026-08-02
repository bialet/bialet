import "_domain" for Task

var id = Request.post("id") || ""
Task.new({"id": id}).toggle()
Response.redirect("/")
