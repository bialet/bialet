import "_app" for Template

if (Request.isPost) {
  var msg = Request.post("msg")
  if (msg) {
    `INSERT INTO messages VALUES (?, CURRENT_TIME)`.query(msg)
    return Template.layout(
      <section class="saved">
        <p>Saved <b>{{ msg }}</b></p>
        <a class="btn" href="/">Back to the guestbook</a>
      </section>
    )
  }
}
return Response.redirect("/?new=1")
