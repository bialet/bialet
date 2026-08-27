// --- Path-Based Route (folder.wren) ---
// Demonstrates: a single file serving both the bare URL and every deeper
// path. users.wren handles /users (list) and /users/:id (detail) by reading
// Request.route(0). See advanced-routing.md for details.

var id = Request.route(0)

if (id == null) {
  var users = `SELECT id, name FROM users ORDER BY name`.fetch
  return <!doctype html>
    <html>
      <head><title>Users</title></head>
      <body style="font: 1.5em/2.5 system-ui; text-align:center">
        <h1>Users</h1>
        {{ users.count > 0 ?
          <ul style="list-style-type:none">
            {{ users.map{|user| <li>
              <a href="/users/{{ user["id"] }}">👤 {{ user["name"] }}</a>
            </li> } }}
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
    <head><title>{{ user["name"] }}</title></head>
    <body style="font: 1.5em/2.5 system-ui; text-align:center">
      <h1>👤 {{ user["name"] }}</h1>
      <p>User id: {{ user["id"] }}</p>
      <p><a href="/users">← Back to users</a></p>
    </body>
  </html>
