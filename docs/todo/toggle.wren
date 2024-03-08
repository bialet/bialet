import "bialet" for Response, Request, Db
import "_domain" for Task

Task.new().toggleFinished(Request.get("id"))
Response.redirect("/")
