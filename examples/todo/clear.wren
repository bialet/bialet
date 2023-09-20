import "bialet" for Response, Db

Db.query("DELETE FROM tasks WHERE finished = 1")
Response.redirect("/")
