import "/_auth" for Auth
import "/_template" for Template

var sess = Session.new()
if (sess.get("loggedIn") == "1") return Response.redirect("/")

var signedOut = Request.get("m") == "signed-out"
var error = null
var userValue = ""

if (Request.isPost) {
  if (!sess.csrfOk) {
    error = "Your session expired. Go back and try again."
  } else {
    userValue = (Request.post("user") || "").trim()
    var pass = Request.post("pass") || ""
    if (userValue == "" || pass == "") {
      error = "Enter both user and password."
    } else if (Auth.attempt(userValue, pass)) {
      sess.set("loggedIn", "1")
      sess.set("loginUser", userValue)
      return Response.redirect("/")
    } else {
      error = "Wrong user or password."
    }
  }
}

var content = <main>
  <section class="card narrow">
    <h1>Sign in</h1>
    {{ signedOut && <div class="flash ok">You have been signed out.</div> }}
    {{ error != null && <div class="flash error">{{ error }}</div> }}
    <form method="post" action="/login" class="stack">
      {{ sess.csrf }}
      <label>User
        <input type="text" name="user" value="{{ userValue }}" autocomplete="username" required autofocus />
      </label>
      <label>Password
        <input type="password" name="pass" autocomplete="current-password" required />
      </label>
      <button type="submit" class="btn btn-primary">Sign in</button>
    </form>
    <p class="muted">The user and password hash are stored in the app config
    (<code>login_user</code> and <code>login_password</code>). Defaults:
    <code>admin</code> / <code>admin123</code>.</p>
  </section>
</main>

return Template.new().layout("Sign in", content, null)
