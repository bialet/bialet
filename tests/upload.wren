if (Request.isPost) {
  var uploadedFile = Request.file("form_file_name")
  if (uploadedFile == null) {
    return Response.end(400, "Upload Error", "No file was uploaded")
  }
  return Response.out("%(uploadedFile.name)|%(uploadedFile.type)|%(uploadedFile.size)")
}

Response.out("upload-ready")
