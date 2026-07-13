if (Request.isPost) {
  if (Request.isJson) {
    var data = Request.json()
    return data["key"]
  } else {
    return "not-json"
  }
} else {
  return "not-post"
}
