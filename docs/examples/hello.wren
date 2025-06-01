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
  html(content) {
    return <!doctype html>
    <html>
      <head>
        <title>{{ _title }}</title>
      </head>
      <body>
        <h1>{{ _title }}</h1>
        {{ content }}
        <p><a href=".">Back ↩️</a></p>
      </body>
    </html>
  }
}

// We use the `get()` method to get the `id` parameter from the URL.
var idUrlParam = Request.get("id")
// Create an instance of the `App` class.
var app = App.new()
// Generate the HTML, with the name of the user.
var html = app.html(
  <p>👋 Hello <b>{{ app.name(idUrlParam) }}</b></p>
)
Response.out(html) // Serve the HTML
