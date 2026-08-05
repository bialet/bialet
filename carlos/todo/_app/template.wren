// The site template — Bialet's answer to header.php + footer.php.
// Static methods, called like Template.layout(html).

class Template {
  static layout(content) {
    return <!doctype html>
    <html lang="en">
      <head>
        <meta charset="utf-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <title>Acme Tasks — internal todo</title>
        <link rel="stylesheet" href="/style.css" />
      </head>
      <body>
        <nav class="nav">
          <span class="brand">Acme Tasks</span>
          <a href="/">All</a>
          <a href="/?filter=open">Open</a>
          <a href="/?filter=done">Done</a>
        </nav>
        <main>{{ content }}</main>
        <footer class="footer">
          <p>Served by one binary. No LAMP stack, no Composer, no Docker.</p>
        </footer>
      </body>
    </html>
  }
}
