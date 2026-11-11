// A counter kept in SQLite. In a real app the table belongs in a migration;
// it is created here so the example runs on its own.
`CREATE TABLE IF NOT EXISTS counter (name TEXT PRIMARY KEY, value INTEGER)`.query
`INSERT OR IGNORE INTO counter (name, value) VALUES ("visits", 0)`.query

`UPDATE counter SET value = value + 1 WHERE name = "visits"`.query
var visits = `SELECT value FROM counter WHERE name = "visits"`.val

return <!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="style.css">
  </head>
  <body>
    <h1>Total times visited: <strong>{{ visits }}</strong></h1>
    <p>Reload the browser or <a href="">click here</a> to see the new value.</p>
    <p><a href=".">Home</a></p>
  </body>
</html>
