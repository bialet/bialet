import "bialet" for Response, Request
import "../_app" for Posts
var id = Request.route(0)

if (id == "") {
  Response.json(Posts.home())
} else {
  Response.json(Posts.id(id))
}
