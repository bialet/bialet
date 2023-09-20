import "bialet" for Response, Request, Db

Db.query("UPDATE tasks
    SET finished = ((finished | 1) - (finished & 1))
    WHERE id = ?", [Request.get("id")])
Response.redirect("/")
