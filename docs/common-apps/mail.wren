import "/_auth" for Auth
import "/_google" for Google
import "/_template" for Template

var guard = Auth.require
if (guard) return guard

var sess = Session.new()
var error = null
var info = null

var account = Google.user
var to = account != null ? account : ""
var subject = ""
var body = ""
var connected = Google.connected

if (Request.isPost) {
  if (!sess.csrfOk) {
    error = "Your session expired. Try again."
  } else {
    to = Util.stripControlChars((Request.post("to") || "").trim())
    subject = Util.stripControlChars(Request.post("subject") || "")
    body = Request.post("body") || ""
    if (to == "") {
      error = "Recipient email is required."
    } else if (subject == "") {
      error = "Subject is required."
    } else {
      var res = Google.mailSend(to, subject, body)
      if (res["ok"]) {
        info = "Email sent to %(to)."
        subject = ""
        body = ""
      } else {
        error = res["error"]
      }
    }
  }
}

var content = <main>
  <h1>Send email</h1>

  {{ !connected && <div class="flash warn">This demo needs a connected Google
    account. <a href="/google/connect">Connect Google</a> first.</div> }}
  {{ info != null && <div class="flash ok">{{ info }}</div> }}
  {{ error != null && <div class="flash error">{{ error }}</div> }}

  <section class="card narrow">
    <h2>Compose</h2>
    {{ connected && account != null && <p class="muted">Sent from
    <strong>{{ account }}</strong>.</p> }}
    <form method="post" action="/mail" class="stack">
      {{ sess.csrf }}
      <label>To
        <input type="email" name="to" value="{{ to }}" required />
      </label>
      <label>Subject
        <input type="text" name="subject" value="{{ subject }}" required />
      </label>
      <label>Body
        <textarea name="body" rows="8">{{ body }}</textarea>
      </label>
      <button class="btn btn-primary">Send</button>
    </form>
    <p class="muted">Sends through the <a
    href="https://developers.google.com/gmail/api/reference/rest/v1/users.messages/send">Gmail
    API</a> as the account you connected with.</p>
  </section>
</main>

return Template.new().layout("Send email", content, Auth.user)
