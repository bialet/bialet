import "bialet" for Response, Request, Db
import "_template" for Template
import "_domain" for Task

if (Request.isPost()) {
  var description = Request.post("task").trim()
  Db.save("tasks", {
    "description": description,
    "finished": false
  })
  return Response.redirect("/")
}

var tasks = Db.all("
  SELECT * FROM tasks
  ORDER BY createdAt ASC
")
tasks.map{ Task.normalizedDescription }

var template = Template.new()

Response.out(template.home(tasks))
