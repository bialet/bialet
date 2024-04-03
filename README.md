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
import "bialet" for Response
// String for the template
var title = "Welcome to Bialet"
// Query object, not a string
var user = `SELECT * FROM users WHERE id = 1`.first()
// Output the template
Response.out('
  <html>
    <head>
      <title>%( title )</title>
    </head>
    <body>
      <h1>%( title )</h1>
      <p>Hello <em>%( user["name"] )</em></p>
    </body>
  </html>
')
```

Bialet is a full-stack web framework that integrates the object-oriented [Wren language](https://wren.io) with a single HTTP server and a built-in SQLite database, creating a unified environment for web development

## Quickstart

1. Use the [Bialet Skeleton](https://github.com/bialet/skeleton) repository with [Docker Compose](https://docs.docker.com/compose/).

```bash
git clone https://github.com/bialet/skeleton.git my-web-app
cd my-web-app
docker compose up
```

2. Visit [localhost:7000](http://localhost:7000) in your browser.

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
- [Codeium](https://github.com/codeium) for the [Codeium](https://codeium.com) plugin, ChatGPT and a lot of coffee.

We encourage users to explore these projects and recognize the efforts of their creators.
