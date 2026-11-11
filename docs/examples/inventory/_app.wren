// Shared layout and session lookup. Files starting with `_` are importable but
// are never served as URLs.

class Auth {
  // The signed-in user, or null. Login stores the id in the session; this
  // reads it back and loads the row it points at.
  static user {
    var id = Session.new().get("user_id")
    if (!id) return null
    return `SELECT id, email FROM users WHERE id = ?`.first(id)
  }
}

class Layout {
  static page(title, user, content) { <!doctype html>
    <html lang="en">
      <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>{{ title }} · Stock</title>
        <link rel="stylesheet" href="/style.css">
      </head>
      <body>
        <header class="bar">
          <a class="brand" href="/items">Stock</a>
          {{ user ? <span class="who">{{ user["email"] }}</span> : "" }}
          <form method="post" action="/logout" class="inline">
            {{ Session.new().csrf }}
            <button class="link">Sign out</button>
          </form>
        </header>
        <main>
          <h1>{{ title }}</h1>
          {{ content }}
        </main>
      </body>
    </html> }
}
