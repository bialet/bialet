import "bialet" for Response, Request, Session, Db
import "_domain" for Task

var id = Request.get("id")
Task.new({"id": id}).toggle()
Response.redirect("/")
