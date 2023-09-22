import "bialet" for Response, Db
import "_domain" for Task

Task.new().clearFinished()
Response.redirect("/")
