import "bialet" for Request, Response

// We create a class that that contains the logic and template.
// This is not required by Bialet, but it is a good practice.
// Once the logic and template become too big, you should split it
// in multiple classes.
class App {
  construct new() {
    _title = "🚲 Welcome to Bialet"
  }
  // Fetch the user from the database using plain SQL
  user(id) { `SELECT * FROM users WHERE id = ?`.first(id) }
  // Get the name from the current user if it exists or the default
  name(id) { user(id)["name"] || "World" }
  // Build the HTML
  html(content) { '
    <html>
      <head>
        <title>%( _title )</title>
      </head>
      <body>
        <h1>%( _title )</h1>
        %( content )
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

// We use the `get()` method to get the `id` parameter from the URL.
var idUrlParam = Request.get("id")
// Create an instance of the `App` class.
var app = App.new()
// Generate the HTML, with the name of the user.
var html = app.html('
  <p>👋 Hello <b>%( app.name(idUrlParam) )</b></p>
')
Response.out(html) // Serve the HTML
