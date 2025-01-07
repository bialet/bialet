# 🚲 [Bialet](https://bialet.dev)

<p align="center">
  <a href="https://bialet.dev">
    <img src="docs/_static/logo.png" alt="" width="200" />
  </a>
</p>
<p align="center">
  <strong>Enhance HTML with a native integration to a persistent database</strong>
</p>

```wren
import "bialet" for Request, Response

class App {
  construct new() {
    _title = "🚲 Welcome to Bialet"
  }
  user(id) { `SELECT * FROM users WHERE id = ?`.first(id) }
  name(id) { user(id)["name"] || "World" }
  html(content) {
    return <!doctype html>
    <html>
      <head>
        <title>{{ _title }}</title>
      </head>
      <body>
        <h1>{{ _title }}</h1>
        {{ content }}
      </body>
    </html>
  }
}

var idUrlParam = Request.get("id")
var app = App.new()
var html = app.html(
  <p>👋 Hello <b>{{ app.name(idUrlParam) }}</b></p>
)
```

Bialet is a full-stack web framework that integrates the object-oriented [Wren language](https://wren.io) with a single HTTP server and a built-in SQLite database, creating a unified environment for web development

## Quickstart

1. Clone or download the [Bialet Skeleton](https://github.com/bialet/skeleton) repository and use [Docker Compose](https://docs.docker.com/compose/) to start the app.

```bash
git clone --depth 1 https://github.com/bialet/skeleton.git mywebapp
cd mywebapp
docker compose up
```

2. Visit [127.0.0.1:7000](http://127.0.0.1:7000) in your browser.

## Build

To build Bialet from source, you'll need to install certain dependencies and run the build process.

### Dependencies

#### Debian/Ubuntu

```bash
sudo apt install -y libsqlite3-dev libssl-dev libcurl4-openssl-dev
```

#### MacOS

```bash
brew install sqlite3 openssl curl
```
### Windows

* libcrypto-3-x64.dll
* libsqlite3-0.dll
* libssl-3-x64.dll

### Building the Project

After installing the dependencies, compile the project and install it:

```bash
make clean && make && make install
```
## License

Bialet is released under the GPL 2 license, ensuring that it remains free and open-source, allowing users to modify and share the software under the same license terms.

## Credits

~I copy a lot of code from all over the web~
Bialet incorporates the work of several open-source projects and contributors. We extend our gratitude to:

- The [Wren programming language](https://wren.io), for its lightweight, flexible, and high-performance capabilities.
- The [Mongoose library](https://github.com/expressjs/mongoose), for providing an easy-to-use web server solution.
- Matthew Brandly, for his invaluable contributions to JSON parsing and utility functions in Wren. Check out his work at [Matthew Brandly's GitHub](https://github.com/brandly/wren-json).
- @PureFox48 for the upper and lower functions.
- @superwills for providing the `getopt` source.
- [Codeium](https://github.com/codeium) for the [Codeium](https://codeium.com) plugin, ChatGPT and a lot of coffee.

We encourage users to explore these projects and recognize the efforts of their creators.
