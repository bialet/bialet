// Reports how many of the 150 file field names g0..g149 the request retained.
// MAX_UPLOAD_FILES caps a single multipart request at 100 file parts, so a body
// with 150 of them must leave exactly 100 rows behind.
var got = 0
for (i in 0...150) {
  if (Request.file("g%(i)")) got = got + 1
}
return "files:%(got)"
