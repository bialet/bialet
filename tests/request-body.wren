if (Request.isPost) {
  if (Request.isJson) {
    var data = Request.json()
    Response.out(data["key"])
  } else {
    Response.out("not-json")
  }
} else {
  Response.out("not-post")
}
