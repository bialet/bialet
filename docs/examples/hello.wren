import "bialet" for Request, Response

class App {
  construct new() {
    _title = "Welcome to Bialet"
    _default = "World"
  }
  // Get the user ID from the URL
  idUrlParam() { Request.get("id") }
  // Fetch the user from the database using plain SQL
  getUser(id) { `SELECT * FROM users WHERE id = ?`.first(id) }
  // Get the name from the current user if it exists or the default
  name() {
    var user = getUser(idUrlParam())
    return user ? user["name"] : _default
  }
  // Build the HTML
  html() { '
    <html>
      <head>
        <title>%( _title )</title>
      </head>
      <body>
        <h1>%( _title )</h1>
        <p>Hello, <em>%( name() )</em>!</p>
      </body>
    </html>
  ' }
}

// We use the `query()` method to execute SQL statements.
// In this case we create a table named `users` with two columns: `id` and `name`.
// This should be do it with migrations, here it is added to the script to be self-contained.
`CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT)`.query()
// Also, we ensure that we have some data in the table.
`REPLACE INTO users (id, name) VALUES (1, "Alice"), (2, "Bob"), (3, "Charlie")`.query()

var app = App.new()
Response.out(app.html()) // Serve the HTML
