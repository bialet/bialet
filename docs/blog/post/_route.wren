import "/_app" for Template, Posts

var slug = Request.route(0)
if (!slug) {
  return Response.redirect("/")
}

var post = Posts.page(slug)
return Template.layout(
  post["title"],
  <div>
    <p><a href="/">Back to home</a></p>
    <h1>{{ post["title"] }}</h1>
    {{ post["content"] }}
  </div>
)
