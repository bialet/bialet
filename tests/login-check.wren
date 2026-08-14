var result = Request.login("admin", "secret")
if (result == false) return "authenticated"
return "unauthorized"
