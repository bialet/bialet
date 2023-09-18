import "bialet" for Response
import "layout" for Layout

System.write("Run docs")

var title = "Documentation"
var content = "
  <p>TODO This will have the API class</p>
"

Response.out(Layout.render(title, content))

