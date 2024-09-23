import "bialet" for Request, Response, Db

if (Request.isPost) {
  var picture = Request.file("picture")
  System.print("File data: %(picture)")
  if (picture["originalFileName"].split(".")[1] == "jpg") {
    Response.header("Content-Type", "image/jpeg")
    return Response.out(picture["file"])
  }
  if (picture["originalFileName"].split(".")[1] == "png") {
    Response.header("Content-Type", "image/png")
    return Response.out(picture["file"])
  }
}

Response.out(
<!doctype html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Upload image</title>
    <style>body{ font: 1.3em system-ui; text-align: center }</style>
  </head>
  <body>
    <h1>Uplad image</h1>
    <form method="post" enctype="multipart/form-data">
      <input type="file" name="picture">
      <input type="submit" value="Upload image">
    </form>
  </body>
</html>
)
