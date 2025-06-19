# About Bialet

**Bialet** is a minimalist full-stack web framework that brings back the joy of writing web apps.

No build steps. No dependencies. Just HTML, SQL, and a lightweight scripting language.

## ✨ Features

- ✅ Built with C for speed and simplicity
- 📝 Write templates and logic in [Wren](https://wren.io/)
- 🗃️ SQLite database included by default
- 🛠️ No YAML, no JSON, no config files
- ⚡ Instant deployment: just copy your files

## 📦 Example

```wren
class App {
  construct new() {
    _title = "Hello from Bialet"
  }

  html() {
    return <!doctype html>
    <html>
      <head><title>{{ _title }}</title></head>
      <body><h1>{{ _title }}</h1></body>
    </html>
  }
}

return App.new().html()
