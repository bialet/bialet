var code = Request.get("code")
if (code == "404") {
  Response.status(404)
  Response.out("not found")
} else if (code == "201") {
  Response.status(201)
  Response.out("created")
} else if (code == "500") {
  Response.status(500)
  Response.out("error")
} else {
  Response.status(200)
  Response.out("ok")
}
