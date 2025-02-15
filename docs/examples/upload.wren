import "bialet" for Request, Response, Db

if (Request.isPost) {
  // Get the uploaded files
  var persistFile = Request.file("persist")
  if (persistFile) {
    System.print("Saving %(persistFile.name)")
  }
  var temporalFile = Request.file("temporal")
  if (temporalFile) {
    System.print("Showing %(temporalFile.name)")
    // Make it temporal, it will be deleted soon
    // You can still use it in the rest of the request
    temporalFile.temp()
    // Return the file to the browser
    return Response.file(temporalFile.id)
  }
}

var title = "Upload Files"
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
    {{ Request.isPost && <h2>Uploaded Files: {{ Request.files.count }}</h2> }}
    <form method="post" enctype="multipart/form-data">
      <p>
        <label for="temporal">Temporal file</label>
        <input type="file" name="temporal" />
      </p>
      <p>
        <label for="persist">Persisted file</label>
        <input type="file" name="persist" />
      </p>
      <input type="submit" value="{{ title }}">
    </form>
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>
)
