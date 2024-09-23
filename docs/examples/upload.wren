import "bialet" for Request, Response, Db

if (Request.isPost) {
  // Get the uploaded file
  var uploadedFile = Request.file("form_file_name")
  System.print("File: %(uploadedFile.name)")
  // Make it temporal, it will be deleted soon
  // You can still use it in the rest of the request
  uploadedFile.temp()
  // Return the file to the browser
  return Response.file(uploadedFile.id)
}

var title = "Upload File"
Response.out(
<!doctype html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>{{ title }}</title>
    <style>body{ font: 1.3em system-ui; text-align: center }</style>
  </head>
  <body>
    <h1>{{ title }}</h1>
    <form method="post" enctype="multipart/form-data">
      <input type="file" name="form_file_name">
      <input type="submit" value="{{ title }}">
    </form>
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>
)
