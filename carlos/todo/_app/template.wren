// Shared layout. Same shape as every layout() I wrote in PHP since 2006.

class Template {
  // Wren has no implicit constructor: Template.new() fails at runtime
  // unless this is declared. Easy to forget if you're used to PHP.
  construct new() {}

  layout(content) { <!doctype html>
    <html lang="en">
      <head>
        <meta charset="utf-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <title>Carlos's Todo</title>
        <link rel="stylesheet" href="/style.css" />
      </head>
      <body>
        <main>
          {{ content }}
        </main>
      </body>
    </html> }
}
