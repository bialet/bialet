import "../_app" for Posts

var id = Request.route(0)

if (!id) {
  System.log("List posts")
  Response.json(Posts.home())
} else {
  System.log("Get post %(id)")
  Response.json(Posts.id(id))
}
