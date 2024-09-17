import "bialet" for Response, Util

var users = `SELECT id, name FROM users`.fetch
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
          {{ users.map{|user| <li>
            <a href="/hello?{{ Util.params({"id": user["id"]}) }}">
              {{ Util.htmlEscape(user["name"]) }}
            </a>
          </li> } }}
        </ul> :
        <p>No users, go to <a href="/hello">hello</a>.</p>
      }}
    </body>
  </html>
)
