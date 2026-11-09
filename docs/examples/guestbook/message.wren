if (Request.isPost) {
  var msg = Request.post("msg")
  if (msg) {
    `INSERT INTO messages VALUES (?, CURRENT_TIME)`.query(msg)
    return <main>
      <p>Messaged <b>{{ msg }}</b> saved!</p>
      <p><a href="/">Show messages</a></p>
    </main>
  }
}
return Response.redirect("/")
