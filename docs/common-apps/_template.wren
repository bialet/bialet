class Template {
  construct new() {}

  layout(title, content, user) { <!doctype html>
    <html lang="en">
      {{ head(title) }}
      <body>
        <header class="topbar">
          <a class="brand" href="/">Common Apps</a>
          <nav class="nav">
            {{ user != null && <a href="/">Dashboard</a> }}
            {{ user != null && <a href="/spreadsheet">Spreadsheet</a> }}
            {{ user != null && <a href="/mail">Send email</a> }}
            {{ user != null && <a href="/google/connect">Google</a> }}
            {{ user == null && <a class="btn" href="/login">Sign in</a> }}
            {{ user != null && <span class="chip">{{ user }}</span> }}
            {{ user != null && <a class="btn btn-outline" href="/logout">Sign out</a> }}
          </nav>
        </header>
        <main class="container">
          {{ content }}
        </main>
        <footer class="footer">
          <p>Common Apps &mdash; a Bialet starter. Read the
          <a href="https://github.com/bialet/bialet/tree/main/docs/common-apps">source code</a>.</p>
        </footer>
      </body>
    </html> }

  head(title) { <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>{{ title }} &middot; Common Apps</title>
    <link rel="stylesheet" href="/style.css" />
  </head> }
}
