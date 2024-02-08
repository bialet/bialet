// This line imports the Response class for managing HTTP interactions.
// The import lines are write at the top of the script.
import "bialet" for Response
import "bialet/extra" for Resource, Auth

// We use the `query()` method to execute SQL statements.
// In this case we create a table named `counter` with two columns: `name` and `value`.
// This should be do it with migrations, here it is added to the script to be self-contained.
`CREATE TABLE IF NOT EXISTS counter (name TEXT PRIMARY KEY, value INTEGER)`.query()

// Authentication, when the user is not authenticated, they are redirected to the login page.
if (Auth.isGuest) return Auth.require()

// When the user is not an admin, they are denied. A forbidden page is shown.
if (!Auth.isAdmin) return Auth.deny()

// Creates a CRUD resource for the `counter` table.
var counterResource = Resource.new('counter')

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
    %( counterResource.html )
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>
')
