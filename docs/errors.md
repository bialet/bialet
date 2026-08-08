# Error Pages

Bialet ships with built-in pages for the situations where no route is found
and for when your code fails. You can replace both with your own HTML files —
no Wren required.

## Default Pages

When a request fails, Bialet returns a plain HTML page:

| Situation                                  | Status | Default page          |
|--------------------------------------------|--------|-----------------------|
| No route matches the URL                   | 404    | Not found page        |
| A `.wren` file fails to parse or throws    | 500    | Internal server error |

The welcome page shown at `/` when the root has no `index.wren` or
`index.html` is a separate page and cannot be customized this way.

## Custom Error Pages

Drop an HTML file named after the status code in your app root directory.
The server looks for it every time that error is produced:

```text
my-site/
├── index.wren
├── 404.html   # served on every 404
└── 500.html   # served on every 500
```

Any file named `<status-code>.html` in the root is used when Bialet renders
that error. In practice that means `404.html` and `500.html`:

```html
<!-- 404.html -->
<!DOCTYPE html>
<html lang="en">
<head><meta charset="UTF-8"><title>Page not found</title></head>
<body>
  <h1>Lost? This page went missing.</h1>
  <p><a href="/">Back to the homepage</a></p>
</body>
</html>
```

When a file is missing or an error occurs, Bialet reads the matching file and
serves it with the correct HTTP status code and `Content-Type: text/html`.

> ⚠️ Pitfall: the `<status>.html` file is only read for errors Bialet
> generates itself. If a `.wren` file calls `Response.status(404)` and
> returns its own body, that body wins and `404.html` is not used.

## Scope

Custom error pages are **static HTML only**. The file is served verbatim:

- No `{{ ... }}` interpolation
- No `_app.wren` template wrapping
- No Wren execution

> ⚠️ Pitfall: don't name the file `500.wren` expecting it to run. Bialet
> does not execute `.wren` error pages; it only reads `.html` files. If your
> error page must be dynamic, serve the error through your own `.wren` route
> with `Response.status(...)` instead.

## Showing Compile Errors in the Browser

By default a `.wren` file that fails to parse or throws returns the generic 500
page, and the actual error goes to the server log. During development it is
often faster to see the error in the browser. `bialet dev` enables this for you
(along with live reload and browser opening) — see
[Getting Started](../getting-started/2-setup). Or enable it manually:

```bash
bialet -r 'Config.enable("BIALET_SHOW_ERRORS")' .
```

Disable it the same way:

```bash
bialet -r 'Config.disable("BIALET_SHOW_ERRORS")' .
```

When enabled, a compile or runtime error renders a page showing the error
type, the module, the line, and the message — instead of the generic 500 page.
This applies even if you have a custom `500.html` or `500.wren`; the error page
wins while the flag is on.

> ⚠️ Pitfall: `BIALET_SHOW_ERRORS` exposes error details to anyone who can
> reach the server. Use it only in development, never in production.
