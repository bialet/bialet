import "_app" for Template, Posts

return Template.layout(
  "Blog Sample",
  <div>
    <h1>Blog Sample</h1>
    <p style="margin-bottom: 2em">
      <a href="/about">About</a>
      is a sample page in Markdown.
    </p>
    {{ Posts.home().map{ |post| <p>
        <a href="/post/{{ post["slug"] }}">{{ post["title"] }}</a>
      </p> } }}
  </div>
)
