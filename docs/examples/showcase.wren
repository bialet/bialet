// This should be do it with migrations, here it is added to the script to be self-contained.
`CREATE TABLE IF NOT EXISTS items (phrase TEXT)`.query

if (Request.isPost) {
  Db.save('items', {"phrase": Request.post("input").trim() })
}
var items = `SELECT * FROM items`.fetch
var title = "🚲 Bialet showcase"
var content = <section>
  <h2>Items</h2>
  {{ items.count > 0 ?
    <ul>
      {{ items.map{|item| <li>{{ item["phrase"] }}</li> } }}
    </ul> :
    <p>No items, create one!.</p>
  }}
</section>

return <!doctype html>
  <html>
    <head>
      <title>{{ title }}</title>
      <meta charset="utf-8">
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <link rel="stylesheet"
        href="https://cdn.jsdelivr.net/npm/@picocss/pico@2/css/pico.fluid.classless.min.css">
    </head>
    <body>
      <main>
        <h1>{{ title }}</h1>
        <form method="post">
          <input name="input" placeholder="Write some text..">
          <input type="submit" value="Add new item">
        </form>
        <hr>
        {{ content }}
      </main>
      <footer>Made with <a href="https://bialet.dev">Bialet</a></footer>
    </body>
  </html>
