var users = `SELECT id, name FROM users`.fetch
var title = "🗂️ Users list"

return <!doctype html>
<html>
  <head><title>{{ title }}</title></head>
  <body style="font: 1.5em/2.5 system-ui; text-align:center">
    <h1>{{ title }}</h1>
    {{ users.count > 0 ?
      <ul style="list-style-type:none">
        {{ users.map{|user| <li>
          <a href="/hello?id={{ user["id"] }}">
            👋 {{ user["name"] }}
          </a>
        </li> } }}
      </ul> :
      /* Users table is empty */
      <p>No users, go to <a href="/hello">hello</a>.</p>
    }}
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>
