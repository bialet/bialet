import "/_auth" for Auth
import "/_google" for Google
import "/_template" for Template

var sess = Session.new()
if (sess.get("loggedIn") != "1") {
  return Response.redirect("/login?m=signed-out")
}

var code = Request.get("code")
var state = Request.get("state")
var oauthError = Request.get("error")

var message = null
var ok = false

if (oauthError != null) {
  message = "Authorization failed or was cancelled: %(oauthError)"
} else if (code == null || code == "") {
  message = "Google did not return an authorization code."
} else if (state == null || state == "" ||
           !Util.secureEquals(state, sess.get("oauth_state"))) {
  message = "The state check failed. Open the Google connection page and try again."
} else {
  var res = Google.exchangeCode(code)
  if (res["ok"]) {
    ok = true
  } else {
    message = res["error"]
  }
}

if (ok) return Response.redirect("/google/connect?connected=1")

var content = <main>
  <section class="card narrow">
    <h1>Google callback</h1>
    <div class="flash error">{{ message }}</div>
    <p><a class="btn" href="/google/connect">Back to Google connection</a></p>
  </section>
</main>

return Template.new().layout("Google callback", content, Auth.user)
