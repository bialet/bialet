// Response header names are case-insensitive: setting the same field twice with
// a different spelling must replace the line, not add a second one. The request
// side is the same table, so a caller's spelling of a request header must also
// find the value the server stored.
var got = Request.header("X-Input") || ""
Response.header("X-Test", "one")
Response.header("x-test", "two")
Response.header("content-type", "text/plain; charset=UTF-8")
return "input:%(got)"
