// Shared layout. Think layout.tsx, but as a Wren method that takes content.
// Tailwind + Alpine from CDNs because this is prototyping. 🔥
// NOTE: the outermost tag here is the doctype — the parser treats it specially,
// so I can use <html>, <head>, <body> freely inside.

class Template {
  construct new() {
    _title = "Elena's Todo"
  }

  layout(content) { <!doctype html>
    <html lang="en">
      {{ head(_title) }}
      <body class="min-h-screen bg-gray-50 antialiased">
        {{ content }}
      </body>
    </html> }

  head(title) { <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>{{ title }}</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <script defer src="https://cdn.jsdelivr.net/npm/alpinejs@3.x.x/dist/cdn.min.js"></script>
  </head> }
}
