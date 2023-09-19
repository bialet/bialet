import "bialet" for Response, Db
import "layout" for Layout

System.write("Run docs")

var id = Db.save("users", {"name":"Albo", "age":30})
var users = Db.all("SELECT * FROM users")

var title = "Documentation"
var content = "
  <p>Saved user id: %( id )</p>
  <h2>Users (%( users.count ))</h2>
  %( users.count > 0 ? "
    <table>
      <tr>
        %( users[0].keys.map { |col| "
          <th>%(col)</th>
        " } )
      </tr>
      %( users.map { |row| "
        <tr>%(row.values.map { |val| "
          <td>%(val)</td>
          " })
        </tr>" } )
    </table>
  " : "
    <p>No users</p>
  ")
"

Response.out(Layout.render(title, content))

