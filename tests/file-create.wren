if (Request.get("create")) {
  var file = File.create("test.txt", "text/plain", "test content")
  Response.out(file.id.toString + "," + file.name + "," + file.type)
} else if (Request.get("get")) {
  var id = Request.get("id")
  if (id) {
    Response.file(id)
  } else {
    Response.out("no id")
  }
} else {
  Response.out("ready")
}
