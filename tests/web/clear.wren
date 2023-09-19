import "bialet" for Response, Db

Db.query("DELETE FROM users")
System.write("Clear all data and go to docs")
Response.redirect("/docs.wren")
