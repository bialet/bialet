// The configured origin/methods/headers form of Response.cors.
if (Response.cors("https://example.com", "GET, POST", "X-Token")) return
return "cors-custom"
