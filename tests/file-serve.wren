// Response.file(id) streams a stored, non-temporary file: the response body is
// the exact bytes and the Content-Type comes from the BIALET_FILES row.
var file = File.create("served.txt", "text/plain", "served-bytes")
if (file) {
  Response.file(file.id)
}
