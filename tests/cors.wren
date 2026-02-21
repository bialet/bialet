if (Response.cors) return

Response.json({"cors": "enabled"})
