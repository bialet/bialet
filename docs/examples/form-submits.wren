// We use the `query` method to execute SQL statements.
// In this case we create a table named `counter` with two columns: `name` and `value`.
// Then we insert a row with the name `form` and a value of 0, if there is no row
// with the name `form`.
// This should be do it with migrations, here it is added to the script to be self-contained.
`CREATE TABLE IF NOT EXISTS counter (name TEXT PRIMARY KEY, value INTEGER)`.query
`INSERT OR IGNORE INTO counter (name, value) VALUES ("form", 0)`.query

// Set the initial value to 1
var value = 1

// We use the `isPost` property to check if the request method is POST.
if (Request.isPost) {
  // We use the `post()` method to get a parameter from the form.
  value = Request.post("value")
  // We use the `query` method to execute SQL statements.
  // In this case we update the value of the `form` row by the value of the `value`
  // parameter.
  // Note the use of the `?` placeholder for the value of the `value` parameter.
  // All the queries are run as a prepared statement. You can't concatenate query strings.
  `UPDATE counter SET value = value + ? WHERE name = "form"`.query(value)
}

// We use the `val()` method to get the value of the first row in the query result.
var total = `SELECT value FROM counter WHERE name = "form"`.val

// We use the `return` to finish the script and send the response to the client.
// The `{{ ... )` syntax is used to interpolate the value of the `total` variable.
// and the `value` variable.
// Apart from the interpolation, the string is regular HTML.
return <!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="style.css">
  </head>
  <body>
    <h1>Total sum: <strong>{{ total }}</strong></h1>
    <form method="POST">
      <input type="text" name="value" value="{{ value }}">
      <button type="submit">Submit</button>
    </form>
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>
