var markdown = "

# Markdown Example

This is written in Markdown.

[Back ↩️](.)

This is the README.md file:
"

return <!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="style.css">
  </head>
  <body>

    {{ Markdown.html(markdown) }}</p>

    <div style="text-align: left">
      {{ Markdown.file("../../README.md") }}
    </div>

  </body>
</html>
