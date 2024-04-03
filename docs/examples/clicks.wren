// This line imports the Response and Request classes for managing HTTP interactions.
// The import lines are write at the top of the script.
import "bialet" for Response, Request

// We use the `query()` method to execute SQL statements.
// In this case we create a table named `counter` with two columns: `name` and `value`.
// Then we insert a row with the name `clicks` and a value of 0, if there is no row
// with the name `clicks`.
// This should be do it with migrations, here it is added to the script to be self-contained.
`CREATE TABLE IF NOT EXISTS counter (name TEXT PRIMARY KEY, value INTEGER)`.query()
`INSERT OR IGNORE INTO counter (name, value) VALUES ("clicks", 0)`.query()

// We use the `get()` method to get a parameter from the URL.
// When the parameter is `count`, we increment the value of the `clicks` row by 1.
if (Request.get("count")) {
  `UPDATE counter SET value = value + 1 WHERE name = "clicks"`.query()
}

// When the parameter is `reset`, we reset the value of the `clicks` row to 0.
if (Request.get("reset")) {
  `UPDATE counter SET value = 0 WHERE name = "clicks"`.query()
}

// We use the `first()` method to get the first row in the query result.
// We use the `["value"]` property to get the value of the `value` column.
var clicks = `SELECT value FROM counter WHERE name = "clicks"`.first()["value"]

// We use the `out()` method to send the response to the client.
// The `%( ... )` syntax is used to interpolate the value of the `clicks` variable.
// Apart from the interpolation, the string is regular HTML.
Response.out('
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="style.css">
  </head>
  <body>
    <h1>Total times clicked: <strong>%( clicks )</strong></h1>
    <p>
      <a href="clicks?count">Click me! 🔺</a>
      •
      <a href="clicks?reset">Reset 🧹</a>
    </p>
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>
')
