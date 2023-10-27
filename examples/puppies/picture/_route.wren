import "bialet" for Request, Response, Db

var id = Request.route(0).split(".")[0]
var picture = Db.byId("pictures", id)

Response.header("Content-Type", picture["file_type"])
Response.out(picture["file_data"])
