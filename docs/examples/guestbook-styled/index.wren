import "_app" for Template

var messages = `
  SELECT * FROM messages ORDER BY created_at DESC
`.fetch

return Template.layout(
  <section>
    <form method="post" action="/message" class="composer">
      <input name="msg" placeholder="Write a message..." required>
      <button class="btn">Post</button>
    </form>
    <ul class="messages {{ Request.get("new") && "is-new" }}">
      {{ messages.map {|m| <li class="message">
        <p>{{ m["text"] }}</p>
        <time>{{ m["created_at"] }}</time>
      </li> } }}
    </ul>
  </section>
)
