import "_app" for Auth

if (!Auth.user) return Response.redirect("/login")

var id = Request.get("id") || ""
// Only photos attached to an item are served, not every row in the file table.
var attached = `SELECT COUNT(*) FROM items WHERE photo_id = ?`.toNum(id)
if (attached == 0) return Response.notFound()

if (!Response.file(id)) return Response.notFound()
return ""
