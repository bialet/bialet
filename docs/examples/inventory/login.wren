import "_app" for Auth, Layout

if (Auth.user) return Response.redirect("/items")

var session = Session.new()
var email = ""
var error = null

if (Request.isPost) {
  email = (Request.post("email") || "").trim()
  var password = Request.post("password") || ""
  if (!session.csrfOk) {
    error = "The form expired. Try again."
  } else {
    var user = `SELECT id, password FROM users WHERE email = ?`.first(email)
    if (user && Util.verify(password, user["password"])) {
      // Rotate before storing the id: a session planted before login is
      // worthless afterwards.
      Session.rotate()
      session.set("user_id", user["id"])
      return Response.redirect("/items")
    }
    error = "Invalid email or password."
  }
}

return Layout.page("Sign in", null,
  <form method="post" class="card narrow">
  {{ error && <p class="error">{{ error }}</p> }}
  {{ session.csrf }}
  <label>Email <input type="email" name="email" value="{{ email }}" required autofocus></label>
  <label>Password <input type="password" name="password" required></label>
  <button>Sign in</button>
  <p class="hint">Demo account: <code>admin@example.com</code> / <code>changeme</code></p>
</form>
)
