class Template {
  construct new() {
    _title = "Todo List"
    _subtitle = "Stay organized, get things done."
  }

  layout(content) { <!doctype html>
    <html lang="en">
      {{ head(_title) }}
      <body>
        <div class="app">
          <header class="app-header">
            <h1 class="app-title"><span class="accent">{{ _title }}</span></h1>
            <p class="app-subtitle">{{ _subtitle }}</p>
          </header>
          {{ content }}
          {{ footer }}
        </div>
      </body>
    </html> }

  head(title) { <head>
    <title>{{ title }}</title>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <link rel="stylesheet" href="/style.css" />
  </head> }

  footer { <footer class="app-footer">
    <p>
      Example made with <a href="https://bialet.dev">Bialet</a>.
      View <a href="https://github.com/bialet/bialet/tree/main/docs/todo">source code</a>.
    </p>
  </footer> }
}
