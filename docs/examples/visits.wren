// This line imports the Response class for managing HTTP interactions.
// The import lines are write at the top of the script.

// We use the `query()` method to execute SQL statements.
// In this case we create a table named `counter` with two columns: `name` and `value`.
// Then we insert a row with the name `visits` and a value of 0, if there is no row
// with the name `visits`.
// This should be do it with migrations, here it is added to the script to be self-contained.
`CREATE TABLE IF NOT EXISTS counter (name TEXT PRIMARY KEY, value INTEGER)`.query
`INSERT OR IGNORE INTO counter (name, value) VALUES ("visits", 0)`.query

// This is the proper start of the script...

// We increment the value of the row with the name `visits` by 1.
`UPDATE counter SET value = value + 1 WHERE name = "visits"`.query

// We use the `val()` method to get the value of the first row in the query result.
var visits = `SELECT value FROM counter WHERE name = "visits"`.val

// We use the `out()` method to send the response to the client.
// The `{{ ... }}` syntax is used to interpolate the value of the `visits` variable.
// Apart from the interpolation, the string is regular HTML.
Response.out(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="style.css">
  </head>
  <body>
    <h1>Total times visited: <strong>{{ visits }}</strong></h1>
    <p>
      ℹ️ Reload the browser or 
      <a href="">click here</a>
      to see the new value.<p>
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>
)
