class Template {
  static layout(title, content) { '
    <html>
      <head>
        <title>%( title )</title>
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
        %( content )
        <p style="margin-top:3em; font-size: .7em">Copyright &copy; 2023 Bialet</p>
      </body>
    </html>
  ' }
}

class Posts {
  static home() { `SELECT * FROM posts ORDER BY createdAt DESC` }
  static page(slug) { `SELECT title, content FROM posts WHERE slug = ?`.set(slug).first }
  static id(id) { `SELECT * FROM posts WHERE id = ?`.set(id).first }
}
