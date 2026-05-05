var boundary = "---------------------------BialetBoundary"
var body = "--%(boundary)\r\n"
body = body + "Content-Disposition: form-data; name=\"form_file_name\"; filename=\"hello.txt\"\r\n"
body = body + "Content-Type: text/plain\r\n\r\n"
body = body + "hello world\r\n"
body = body + "--%(boundary)--\r\n"

Test.new()
  .route("/upload")
  .method("POST")
  .setHeader("Content-Type", "multipart/form-data; boundary=%(boundary)")
  .postData(body)
  .status(200)
  .contains("hello.txt|text/plain|11")
