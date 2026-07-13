if (Request.get("mode") == "redirect") {
  return Response.redirect(Request.get("to"))
}

if (Request.get("mode") == "end") {
  return Response.end(418, Request.get("title"), Request.get("message"))
}

return "security-ready"
