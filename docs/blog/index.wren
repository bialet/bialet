import "_app" for Template, Posts

return Template.layout(
  "Blog Sample",
  <div>
    <h1>Blog Sample</h1>
    {{ Posts.home().map{ |post| <p>
        <a href="/post/{{ post["slug"] }}">{{ post["title"] }}</a>
      </p> } }}
  </div>
)
