import "bialet" for Response, Request, User
import "_template" for Template
import "_domain" for Task

var task = Task.new()
var hash = User.hash("Hello World")
System.print("hash: %( hash ) - verify: %( User.verify(password, hash) )")

if (Request.isPost()) {
  task.save(Request.post("task"))
  return Response.redirect("/")
}

var template = Template.new()
var tasks = task.list()
Response.out(template.home(tasks))
