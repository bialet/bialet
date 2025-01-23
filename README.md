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

var users = `SELECT id, name FROM users`.fetch
var TITLE = "🗂️ Users list"

Response.out(
  <!doctype html>
  <html>
    <head><title>{{ TITLE }}</title></head>
    <body style="font: 1.5em/2.5 system-ui; text-align:center">
      <h1>{{ TITLE }}</h1>
      {{ users.count > 0 ?
        <ul style="list-style-type:none">
          {{ users.map{|user| <li>
            <a href="/hello?id={{ user["id"] }}">
              👋 {{ user["name"] }}
            </a>
          </li> } }}
        </ul> :
        /* Users table is empty */
        <p>No users, go to <a href="/hello">hello</a>.</p>
      }}
    </body>
  </html>
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

Bialet is released under the MIT license, allowing users to freely use, modify, and distribute the software with fewer restrictions.

## Credits

~I copy a lot of code from all over the web~
Bialet incorporates the work of several open-source projects and contributors. We extend our gratitude to:

- The [Wren programming language](https://wren.io), for its lightweight, flexible, and high-performance capabilities.
- Matthew Brandly, for his invaluable contributions to JSON parsing and utility functions in Wren. Check out his work at [Matthew Brandly's GitHub](https://github.com/brandly/wren-json).
- @PureFox48 for the upper and lower functions.
- @superwills for providing the `getopt` source.
- [Codeium](https://github.com/codeium) for the [Codeium](https://codeium.com) plugin, ChatGPT and a lot of coffee.

We encourage users to explore these projects and recognize the efforts of their creators.
