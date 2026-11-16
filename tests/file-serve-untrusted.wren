// The uploader controls the part's Content-Type, so Response.file must not
// hand it back as HTML in the app's origin. Only script-free media types are
// served inline; anything else downloads as an opaque blob.
var file = File.create("payload.html", "text/html", "<script>alert(1)</script>")
if (file) {
  Response.file(file.id)
}
