// Bialet is a single-binary web framework: Wren scripting + SQLite, file-based routing.
// Drop `.wren` files in a folder, run `bialet`, and you have a server-rendered app.
var title = "🚲 Welcome to Bialet showcase"

// Normally, table creation goes in `_migration.wren` (run once at startup).
// Placed here so this showcase file is fully self-contained.
`CREATE TABLE IF NOT EXISTS items (phrase TEXT)`.query

// POST handling
if (Request.isPost) {
  `items`.save({"phrase": Request.post("input").trim() })
  System.log('✅ Item saved')
}

// Querying
var items = `SELECT * FROM items`.fetch

// Inline HTML with `{{ … }}`
var content = <section>
  <h2>Items</h2>
  {{ items.count > 0 ?
    <ul>
      {{ items.map{|item| <li>{{ item["phrase"] }}</li> } }}
    </ul> :
    <p>No items, create one!.</p>
  }}
</section>

// Every `.wren` file is a handler; `return` sends the response body to the client.
// Use `<!doctype html>` for full HTML pages.
return <!doctype html>
  <html>
    <head>
      <title>{{ title }}</title>
      <meta charset="utf-8">
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/@picocss/pico@2/css/pico.fluid.classless.min.css">
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
