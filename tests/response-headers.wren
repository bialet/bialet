Response.addCookieHeader("tracker=1; Path=/")
Response.header("X-Custom", "custom-value")
var h = Response.headers
return "custom:" + h.contains("X-Custom: custom-value").toString + "|cookie:" + h.contains("Set-Cookie: tracker=1").toString
