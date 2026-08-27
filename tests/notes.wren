var id = Request.route(0)
if (id == null) return "notes-list"
return "note:" + id
