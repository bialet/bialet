var code = Request.get("code")
if (code == "404") {
  Response.status(404)
  return "not found"
} else if (code == "201") {
  Response.status(201)
  return "created"
} else if (code == "500") {
  Response.status(500)
  return "error"
} else {
  Response.status(200)
  return "ok"
}
