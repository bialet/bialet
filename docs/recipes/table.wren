// This line imports the Response class for managing HTTP interactions.
// The import lines are write at the top of the script.
import "bialet" for Response

// We use the `query()` method to execute SQL statements.
// In this case we create a table named `counter` with two columns: `name` and `value`.
// This should be do it with migrations, here it is added to the script to be self-contained.
`CREATE TABLE IF NOT EXISTS counter (name TEXT PRIMARY KEY, value INTEGER)`.query()

// We use the `fetch()` method to get the query result as an array of objects.
var counters = `SELECT * FROM counter ORDER BY name ASC`.fetch()

// We use the `out()` method to send the response to the client.
// The `%( ... )` syntax is used to interpolate the value of the `counters` variable.
// and the `count` variable.
//
// The ternary operator is used to check if the `counters` array is empty or not.
// That's the way to have an IF statement in the interpolated string.
//
// The `map` method is used to iterate over the `counters` array.
// That's the way to have a FOR statement in the interpolated string.
//
// Apart from the interpolation, the strings are regular HTML.
Response.out('
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="style.css">
  </head>
  <body>
    <h1>Show all the counters saved in the database</h1>
    %( /* The interpolated string can have comments on it */
      counters.count > 0 ? '
      <table>
        <tr>
          <th>Name</th>
          <th>Value</th>
        </tr>
        %( /* List all the counters */
          counters.map{|counter| '
          <tr>
            <td>%(counter["name"])</td>
            <td>%(counter["value"])</td>
          </tr>
        '})
      </table>
      ' : /* If there are no counters */ '
        <p>No counters found</p>
      ')
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>
')
