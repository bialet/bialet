import "bialet" for Response, Request, Http
import "_template" for Template
import "_domain" for Task

var task = Task.new()

if (Request.isPost()) {
  task.save(Request.post("task"))
  return Response.redirect("/")
}

var template = Template.new()
var tasks = task.list()
Response.out(template.home(tasks))
