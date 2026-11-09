class Template {
  static layout(content) { <!doctype html>
    <html lang="en">
      <head>
        <meta charset="utf-8">
        <title>Bialet guestbook</title>
        <link rel="stylesheet" href="/css/style.css">
      </head>
      <body>
        <main class="card">
          <h1>Say something nice</h1>
          <p class="sub">Server-rendered, stored in SQLite.</p>
          {{ content }}
        </main>
      </body>
    </html> }
}
