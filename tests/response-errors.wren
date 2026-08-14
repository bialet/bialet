if (Request.get("forbidden")) {
  Response.forbidden()
  return
}
if (Request.get("notfound")) {
  Response.notFound()
  return
}
Response.internalError()
return
