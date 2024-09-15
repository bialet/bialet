var title = "🚲 Users"
var users = `SELECT name FROM users`.fetch

<html>
  <head>
    <title>{{ title }}</title>
  </head>
  <body>
    <h1>{{ title }}</h1>
    <ul>
      {{ users.map{|user| <li>{{ user["name"] }}</li> } }}
    </ul>
  </body>
</html>
