import "bialet" for Request, Response, Db

if (Request.isPost()) {
  var picture = Request.file("picture")
  if (picture) {
    Db.insert("pictures", {
      "file_extension": picture.name.split(".")[-1],
      "file_type": picture.type,
      "file_data": picture.data
    })
  }
}

var pictures = Db.all("SELECT id, file_extension FROM pictures")

Response.out("
<!DOCTYPE html>
  <head>
    <meta charset='utf-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <link rel='icon' href='data:image/svg+xml,<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\"><text y=\".9em\" font-size=\"90\">🐶</text></svg>'>
    <title>Puppies!</title>
    <style>body{ font: 1.3em sans-serif }</style>
  </head>
  <body>
    <h1>🐶 Puppies!</h1>
    <form action='/' method='post' enctype='multipart/form-data'>
      <input type='file' name='picture'>
      <input type='file' name='conter_viejo'>
      <input type='submit' value='Upload picture'>
    </form>
    <ul>
      %( pictures.map{|p| "<li><img src='picture/%( p["id"] ).%( p["file_extension"] )'></li>" })
    </ul>
  </body>
</html>
")
