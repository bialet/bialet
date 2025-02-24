import "bialet" for Request, Response, Db
import "random" for Random

var selectedShow = false

if (Request.isPost) {
  selectedShow = Request.post("show")
  System.print("Selected show: %(selectedShow)")
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
    if (selectedShow == "temporal") {
    // Return the file to the browser
      return Response.file(temporalFile.id)
    }
  }
  if (persistFile && selectedShow == "persist") {
    // Return the file to the browser
    return Response.file(persistFile.id)
  }
}

if (!selectedShow) selectedShow = "temporal"

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
      <input type="hidden" name="test_hidden" value="{{ Random.new().int(1, 10) }}" />
      <p>
        <label for="temporal">Temporal file</label>
        <input type="file" name="temporal" />
      </p>
      <p>
        <label for="persist">Persisted file</label>
        <input type="file" name="persist" />
      </p>
      <p>
        <label>
        <input type="radio" name="show" value="temporal" {{ selectedShow == "temporal" && "checked" }} />
        Show temporal file
        </label>
      </p>
      <p>
        <label>
        <input type="radio" name="show" value="persist" {{ selectedShow == "persist" && "checked" }} />
        Show persisted file
        </label>
      </p>
      <p>
        <label>
        <input type="radio" name="show" value="none" {{ selectedShow == "none" && "checked" }} />
        Show nothing
        </label>
      </p>
      <input type="submit" value="{{ title }}">
    </form>
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>
)
