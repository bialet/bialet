// This line imports the Response and Request classes for managing HTTP interactions.
// It also imports the Session class, that includes the Cross-Site Request Forgery functions.
// The import lines are write at the top of the script.
import "bialet" for Request, Response, Session

var session = Session.new()
var verify

// We use the `isPost` property to check if the request method is POST
if (Request.isPost) {
  // Verify that the CSRF token is valid
  verify = session.csrfOk
}

// We use the `out()` method to send the response to the client.
// The `%( ... )` syntax is used to interpolate the value of the `clicks` variable.
// The `session.csrf` generates the hidden input field with the CSRF token.
// Apart from the interpolation, the string is regular HTML.
Response.out(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="style.css">
  </head>
  <body>
    <h1>Cross-Site Request Forgery</h1>
    <p>Use different tabs to verify the post is from the last window.</p>
    {{ Request.isPost &&
      <p><strong>{{ verify ? 'The post is valid' : 'The post come from someone else' }}</strong></p>
    }}
    <form method="POST">
      {{ session.csrf }}
      <p>
        <button>Submit</button>
      </p>
    </form>
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>)
