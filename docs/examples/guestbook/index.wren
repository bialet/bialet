var messages = `
  SELECT * FROM messages ORDER BY created_at ASC
`.fetch

return <main>
  <h1>Bialet Guestbook</h1>
  <form method="post" action="/message">
    <input name="msg" placeholder="Write a message...">
    <button>Submit</button>
  </form>
  <hr>
  <ul>
    {{ messages.map {|m| <li>{{ m["text"] }}</li> } }}
  </ul>
</main>
