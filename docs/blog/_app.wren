class Template {
  static layout(title, main) { <!doctype html><html>
      <head>
        <title>{{ title }}</title>
        <meta charset="utf-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <style>
          body {
            font-family: -apple-system, BlinkMacSystemFont, "Roboto", "Helvetica Neue", sans-serif;
            font-size: 1.2em;
            margin: 2em;
          }
          a { color: #0070f3; text-decoration: none; }
          a:hover { text-decoration: underline; }
        </style>
      </head>
      <body>
        {{ main }}
        <p style="margin-top:3em; font-size: .7em">
          Made with <a href="https://bialet.dev">Bialet</a>.
          View <a href="https://github.com/bialet/bialet/tree/main/docs/blog">source code</a>.
      </body>
    </html> }
}

class Posts {
  static all() { `SELECT * FROM posts ORDER BY createdAt DESC`.fetch }
  static one(id) { `SELECT * FROM posts WHERE id = ?`.first(id) }
  static page(slug) { `SELECT id, title, content FROM posts WHERE slug = ?`.first(slug).to(Post) }
  static home() { all().to(Post) }
  static id(id) { one(id).to(Post) }
}

class Post {
  construct new(data) {
    _id = data["id"]
    _title = data["title"]
    _content = data["content"]
    _slug = data["slug"]
    _createdAt = data["createdAt"]
  }
  id { _id }
  title { _title }
  content { _content }
  slug { _slug }
  createdAt { _createdAt }
}
