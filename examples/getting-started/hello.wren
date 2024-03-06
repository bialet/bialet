import "bialet" for Request, Response

var name = Request.get("name")

Response.out('<p>Hello %( name || "World" )!</p>')
