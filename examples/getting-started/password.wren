// This line imports the Response and Request classes for managing HTTP interactions.
// It also imports the Util class, that includes the helper functions.
// The import lines are write at the top of the script.
import "bialet" for Request, Response, Util

// We use the `post()` method to get a parameter from the POST request.
var password = Request.post('password')
var passwordCheck = Request.post('password-check')
// Set up the hash and verify variables
var encrypted
var verify

// We use the `isPost` property to check if the request method is POST
if (Request.isPost) {
  // Hash the password. The hash is a string that includes the salt.
  // The salt is randomly generated. The same password will have a different salt each time the `hash()` method is called.
  // The hash use SHA-256 to encode the password with the salt.
  encrypted = Util.hash(password)
  // Verify the password against the hash
  verify = Util.verify(passwordCheck, encrypted)
}

// We use the `out()` method to send the response to the client.
// The `%( ... )` syntax is used to interpolate the value of the `clicks` variable.
// Apart from the interpolation, the string is regular HTML.
Response.out('
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="style.css">
  </head>
  <body>
    <h1>Password utilities</h1>
    %( Request.isPost ? '
      <p>Hash:</p>
      <code>%( encrypted )</code>
      <p><strong>%( verify ? 'The passwords are the same' : 'The passwords are different' )</strong></p>
      <hr />
    ' : '' )
    <form method="POST">
      <p>
        <label for="password">Password</label>
      </p>
      <p>
        <input type="text" name="password" value="%(password)" />
      </p>
      <p>
        <label for="password-check">Password to check</label>
      </p>
      <p>
        <input type="text" name="password-check" value="%(passwordCheck)" />
      </p>
      <p>
        <button>Submit</button>
      </p>
    </form>
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>
')
