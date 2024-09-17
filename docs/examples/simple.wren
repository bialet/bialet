import "bialet" for Response

var users = `SELECT name FROM users`.fetch
var title = "Users list"

Response.out(
  <!doctype html>
  <html>
    <head>
      <title>{{ title }}</title>
      <style>
        body { font: 1.5em/2.5 system-ui; text-align: center }
        ul { list-style-type: none }
      </style>
    </head>
    <body>
      <h1>{{ title }}</h1>
      {{ users.count > 0 ?
        <ul>
          {{ users.map{|user| <li>{{ user["name"] }}</li> } }}
        </ul> :
        <p>No users.</p>
      }}
    </body>
  </html>
)
