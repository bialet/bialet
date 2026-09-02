import "/_auth" for Auth
import "/_google" for Google
import "/_template" for Template

var guard = Auth.require
if (guard) return guard

var sess = Session.new()
var error = null

if (Request.isPost) {
  if (!sess.csrfOk) {
    error = "Your session expired. Go back and try again."
  } else {
    var action = Request.post("action") || ""
    if (action == "save") {
      var id = (Request.post("client_id") || "").trim()
      var secret = (Request.post("client_secret") || "").trim()
      var appUrl = (Request.post("app_url") || "").trim()
      if (id == "" || secret == "") {
        error = "Client ID and Client Secret are required."
      } else {
        Config.set("google_client_id", id)
        Config.set("google_client_secret", secret)
        if (appUrl != "") Config.set("app_url", appUrl)
        return Response.redirect("/google/connect?saved=1")
      }
    } else if (action == "disconnect") {
      Google.disconnect()
      return Response.redirect("/google/connect?disconnected=1")
    }
  }
}

var notice = null
if (Request.get("saved") == "1") {
  notice = "Google credentials saved. Connect your account below."
} else if (Request.get("connected") == "1") {
  notice = "Google account connected."
} else if (Request.get("disconnected") == "1") {
  notice = "Google account disconnected."
}

var configured = Google.configured
var connected = Google.connected
var account = Google.user
var authUrl = ""
if (configured) {
  var state = Util.randomString(32)
  sess.set("oauth_state", state)
  authUrl = Google.authorizeUrl(state)
}

var statusLabel
if (connected) {
  statusLabel = account != null ? "Connected as %(account)" : "Connected"
} else if (configured) {
  statusLabel = "Configured, not connected"
} else {
  statusLabel = "Not configured"
}

var content = <main>
  <h1>Google connection</h1>

  {{ notice != null && <div class="flash ok">{{ notice }}</div> }}
  {{ error != null && <div class="flash error">{{ error }}</div> }}

  <section class="card">
    <h2>Status</h2>
    <p class="status {{ connected ? "ok" : "warn" }}">{{ statusLabel }}</p>
    {{ configured && connected && <form method="post" class="row">
      {{ sess.csrf }}
      <input type="hidden" name="action" value="disconnect" />
      <button class="btn btn-outline">Disconnect this account</button>
    </form> }}
    {{ configured && !connected && <a class="btn btn-primary" href="{{ authUrl }}">Connect to Google</a> }}
  </section>

  <section class="card">
    <h2>OAuth credentials</h2>
    <p>In <a href="https://console.cloud.google.com">Google Cloud Console</a>:</p>
    <ol>
      <li>Create an OAuth client of type <strong>Web application</strong>.</li>
      <li>Enable the <strong>Google Sheets API</strong> and the
      <strong>Gmail API</strong> for the project.</li>
      <li>Add this exact redirect URI to the client:
      <code class="mono">{{ Google.redirectUri }}</code></li>
      <li>Paste the client ID and secret below.</li>
    </ol>
    <form method="post" class="stack">
      {{ sess.csrf }}
      <input type="hidden" name="action" value="save" />
      <label>Client ID
        <input type="text" name="client_id" value="{{ Google.clientId }}" autocomplete="off" required />
      </label>
      <label>Client Secret
        <input type="password" name="client_secret" value="{{ Google.clientSecret }}" autocomplete="off" required />
      </label>
      <label>App base URL (used to build the redirect URI)
        <input type="text" name="app_url" value="{{ Google.appUrl }}" />
      </label>
      <button class="btn">Save credentials</button>
    </form>
    <p class="muted">Credentials and tokens are stored in the app config
    store. Keep <code>_db.sqlite3</code> out of version control.</p>
  </section>
</main>

return Template.new().layout("Google connection", content, Auth.user)
