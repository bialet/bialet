// We create a class that that contains the logic and template.
// This is not required by Bialet, but it is a good practice.
// Once the logic and template become too big, you should split it
// in multiple classes.
class App {
  // The name of the constructor could be anything.
  construct new() {
    _title = "🚲 Welcome to Bialet"
  }
  // Fetch the user's name from the database using plain SQL
  // or return "World" if the user doesn't exist.
  // Single line methods return the value of the expression.
  name(id) { `SELECT name FROM users WHERE id = ?`.val(id) || "World" }
  // Build the HTML
  html(content) {
    return <!doctype html>
    <html>
      <head>
        <title>{{ _title }}</title>
      </head>
      <body style="font: 1.5em/2.5 system-ui; text-align:center">
        <h1>{{ _title }}</h1>
        {{ content }}
        <p><a href="simple">Back to List 👥</a></p>
        <p><a href=".">to Home ↩️</a></p>
      </body>
    </html>
  }
}

// We use the `get()` method to get the `id` parameter from the URL.
var id = Request.get("id")
// Create an instance of the `App` class.
var app = App.new()
// Generate the HTML, with the name of the user.
return app.html(<p>👋 Hello <b>{{ app.name(id) }}</b></p>)
