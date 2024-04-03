// This line imports the Response class for managing HTTP interactions.
// The import lines are write at the top of the script.
import "bialet" for Request, Response, Util
import "bialet/extra" for Resource, Auth, User

var user = User.new()
user.id = 1
user.name = "admin"
user.password = "admin"
user.isAdmin = true
user.save()

System.print(Request.header("authorization"))

// Authentication, when the user is not authenticated, they are redirected to the login page.
if (!Auth.user) return Auth.require()

// When the user is not an admin, a forbidden page is shown.
if (!Auth.user.isAdmin) return Auth.deny()

// You can also set the `Auth.denied` variable.
Auth.denied = !Auth.user.isAdmin
// then use the check method, it will redirect to a login or show a forbidden page
// if the Auth denied variable is true.
if (Auth.check) return

// Creates a CRUD resource for the `counter` table.
var counterResource = Resource.new('counter')

// If the request is a JSON request, it will handle the CRUD operations on the `counter` table
// using the proper HTTP verbs to do the operations and passing the ID of the resource via GET.
if (counterResource.json) return

Response.out('
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="style.css">
  </head>
  <body>
    <h1>A fully CRUD generator</h1>
    <h2>Hi %(Auth.user.name)!</h2>
    %( counterResource.html )
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>
')
