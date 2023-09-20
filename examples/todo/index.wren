import "bialet" for Response, Request, Db
import "_template" for Template

if (Request.method() == "POST") {
  Db.save("tasks", {
    "description": Request.post("task").trim(),
    "finished": false
  })
}

var tasks = Db.all("SELECT * FROM tasks")
var template = Template.new()

Response.out(template.home(tasks))
