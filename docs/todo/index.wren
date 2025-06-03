import "bialet" for Response, Request
import "_template" for Template
import "_domain" for Task

if (Request.isPost) {
  var task = Task.new()
  task.description = Request.post("task")
  task.save()
  return Response.redirect("/")
}

var template = Template.new()
return template.home(Task.list())
