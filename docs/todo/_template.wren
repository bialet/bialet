import "bialet" for Date

class Template {
  construct new() {
    _title = "🥳 TODO List"
  }

  layout(content) { <!doctype html>
    <html>
      {{ head(_title) }}
      <body>
        <h1>{{ _title }}</h1>
        {{ content }}
        {{ footer }}
      </body>
    </html> }

  head(title) { <head>
    <title>{{ title }}</title>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <link rel="stylesheet" href="/style.css" />
  </head> }

  footer { <footer>
    <p>
      Example made with <a href="https://bialet.dev">Bialet</a>.
      View <a href="https://github.com/bialet/bialet/tree/main/docs/todo">source code</a>.
    </p>
  </footer> }
}
