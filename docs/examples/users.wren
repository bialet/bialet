// One file serves /users (the list) and /users/:id (the detail page):
// Request.route(0) is the first path segment after the file name. This is the
// SEO-friendly alternative to /users?id=…, covered in Advanced Routing.

var id = Request.route(0)

if (id == null) {
  var users = `SELECT id, name FROM users ORDER BY name`.fetch
  return <!doctype html>
    <html>
      <head>
        <title>Users</title>
        <link rel="stylesheet" href="../style.css">
      </head>
      <body>
        <h1>Users</h1>
        {{ users.count > 0 ?
          <ul>
            {{ users.map{|user| <li><a href="/users/{{ user["id"] }}">{{ user["name"] }}</a></li> } }}
          </ul> :
          <p>No users yet.</p>
        }}
      </body>
    </html>
}

var user = `SELECT id, name FROM users WHERE id = ?`.first(id)
if (!user) return Response.notFound()

return <!doctype html>
  <html>
    <head>
      <title>{{ user["name"] }}</title>
      <link rel="stylesheet" href="../style.css">
    </head>
    <body>
      <h1>{{ user["name"] }}</h1>
      <p>User id: {{ user["id"] }}</p>
      <p><a href="/users">Back to users</a></p>
    </body>
  </html>
