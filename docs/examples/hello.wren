// A class keeps the query and the template together. Not required, but a good
// habit once a page outgrows a few lines.
class App {
  construct new() {
    _title = "Welcome to Bialet"
  }

  // One line, one query. The last expression is the return value.
  name(id) { `SELECT name FROM users WHERE id = ?`.val(id) || "World" }

  html(content) {
    return <!doctype html>
    <html>
      <head>
        <title>{{ _title }}</title>
        <link rel="stylesheet" href="style.css">
      </head>
      <body>
        <h1>{{ _title }}</h1>
        {{ content }}
        <p><a href="simple">Back to the list</a></p>
        <p><a href=".">Home</a></p>
      </body>
    </html>
  }
}

var id = Request.get("id")
var app = App.new()
return app.html(<p>Hello <b>{{ app.name(id) }}</b></p>)
