import "../_app" for Posts

var id = Request.route(0)

if (!id) {
  System.log("List posts")
  Response.json(Posts.all())
} else {
  System.log("Get post %(id)")
  Response.json(Posts.one(id))
}
