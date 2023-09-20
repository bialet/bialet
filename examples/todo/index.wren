import "bialet" for Response, Request, Db
import "_templates" for TodoTemplate

if (Request.method() == "POST") {
  Db.save("tasks", {"description": Request.post("task"), "finished": false})
}

TodoTemplate.list = []
TodoTemplate.list = Db.all("SELECT * FROM tasks")

Response.out(TodoTemplate.layout)
