// Using GitHub
import "gh:4lb0/emoji/emoji" for Emoji

// Using URL
import "https://raw.githubusercontent.com/4lb0/emoji/5a3c800b210cf366678cb5c94150c8a183b3de3f/favicon.wren" for Favicon

return <!doctype html>
<html>
  <head>
    <title>{{ Emoji.rocket }}</title>
    {{ Favicon.star }}
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="style.css">
  </head>
  <body>
    <h1>{{ Emoji.heart }}</h1>
    <p><a href=".">Back ↩️</a></p>
  </body>
</html>
