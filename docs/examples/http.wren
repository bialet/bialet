// This line imports the Response class for managing HTTP interactions.
// The import lines are write at the top of the script.

var users = Http.get('https://dummyjson.com/users?limit=5&select=username,email')['users']
System.print("Users: %(users)")

Response.out(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="style.css">
  </head>
  <body>
    <h1>Show all the users from the JSONPlaceholder API</h1>
    {{ /* The interpolated string can have comments on it */
      users.count > 0 ?
      <table>
        <tr>
          <th>Name</th>
          <th>Email</th>
        </tr>
        {{ /* List all the users */
          users.map{|user| <tr>
            <td>{{ user["username"] }}</td>
            <td>{{ user["email"] }}</td>
          </tr> } }}
      </table> :
        /* If there are no users */
        <p>No user found</p>
      }}
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>)
