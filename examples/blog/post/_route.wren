import "bialet" for Response, Request
import "/_app" for Template, Posts
var slug = Request.route(0)

if (slug == "") {
  return Response.redirect("/")
}

var post = Posts.page(slug)

Response.out(Template.layout(post["title"], '
  <p><a href="/">Back to home</a></p>
  <h1>%( post["title"] )</h1>
  %( post["content"] )
'))
