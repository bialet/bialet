var session = Session.new()
var verify

// We use the `isPost` property to check if the request method is POST
if (Request.isPost) {
  // Verify that the CSRF token is valid
  verify = session.csrfOk
}

return <!doctype html>
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
</html>
