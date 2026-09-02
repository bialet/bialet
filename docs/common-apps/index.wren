import "/_auth" for Auth
import "/_google" for Google
import "/_template" for Template

var guard = Auth.require
if (guard) return guard

var connected = Google.connected
var account = Google.user
var connLabel
if (connected) {
  connLabel = account != null ? "Connected as %(account)" : "Connected"
} else if (Google.configured) {
  connLabel = "Not connected yet"
} else {
  connLabel = "Google not configured"
}

var content = <main>
  <section class="hero">
    <h1>Common Apps</h1>
    <p>Signed in as <strong>{{ Auth.user }}</strong>. This Bialet starter
    combines a config-based login with Google OAuth2 to read spreadsheets and
    send email.</p>
  </section>

  <section class="cards">
    <div class="card">
      <h2>Google connection</h2>
      <p class="status {{ connected ? "ok" : "warn" }}">{{ connLabel }}</p>
      <p>OAuth2 authorization-code flow. Tokens are kept in the config store
      and access tokens refresh automatically.</p>
      <a class="btn" href="/google/connect">Manage connection</a>
    </div>

    <div class="card">
      <h2>Spreadsheet</h2>
      <p>Read a range from a Google Sheet or append rows to it through the
      Sheets API v4.</p>
      <a class="btn" href="/spreadsheet">Open spreadsheet demo</a>
    </div>

    <div class="card">
      <h2>Send email</h2>
      <p>Send email from the connected account through the Gmail API.</p>
      <a class="btn" href="/mail">Open mail demo</a>
    </div>
  </section>
</main>

return Template.new().layout("Dashboard", content, Auth.user)
