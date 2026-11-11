var users = `SELECT id, name FROM users`.fetch

return <!doctype html>
<html>
  <head>
    <title>Users</title>
    <link rel="stylesheet" href="style.css">
  </head>
  <body>
    <h1>Users</h1>
    {{ users.count > 0 ?
      <ul>
        {{ users.map{|user| <li><a href="/hello?id={{ user["id"] }}">{{ user["name"] }}</a></li> } }}
      </ul> :
      <p>No users yet. Try <a href="/hello">hello</a>.</p>
    }}
    <p><a href=".">Home</a></p>
  </body>
</html>
