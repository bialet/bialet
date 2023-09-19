import "bialet" for Response, Request
import "layout" for Layout

System.write("Run index")
System.write(Request.get("nombre"))

var title = "Welcome to Bialet"
var content = "
  <p>Create a file called <strong>index.wren</strong></p>
  <code>
    Response.out(\"Hello World!\")
  </code>
  <p>Then cd to the folder of the web and run</p>
  <code>bialet</code>
"

Response.out(Layout.render(title, content))
