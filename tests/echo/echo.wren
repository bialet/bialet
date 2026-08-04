if (Request.isPost) return "POST"
if (Request.method == "PUT") return "PUT"
if (Request.method == "DELETE") return "DELETE"
return "GET"
