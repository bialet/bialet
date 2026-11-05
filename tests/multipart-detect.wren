// The C multipart detector and the app-visible header map must read one header
// block. A whitespace-only line used to end the map early while the C scan read
// past it, so a request the app saw as text/plain still delivered multipart
// fields and files; a space before the colon did the opposite.
var ct = Request.header("content-type") || "none"
var note = Request.post("note") || "none"
var file = Request.file("doc")
var f = file ? "%(file.name):%(file.size)" : "none"
return "ct:%(ct)|note:%(note)|file:%(f)"
