import "bialet" for Response, Request, Db
import "_domain" for Task

Task.new(Request.get("id")).toggleFinished()
Response.redirect("/")
