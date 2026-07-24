# 🚲 [Bialet](https://bialet.dev)

<p align="center">
  <a href="https://bialet.dev">
    <img src="docs/_static/logo.png" alt="" width="200" />
  </a>
</p>
<p align="center">
  <strong>Enhance HTML with a native integration to a persistent database</strong>
</p>

<p align="center">
  <a href="https://github.com/bialet/bialet/releases"><img src="https://img.shields.io/github/v/release/bialet/bialet?color=%237c3aed&label=version" alt="Version"></a>
  <a href="https://github.com/bialet/bialet/blob/main/LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License"></a>
  <a href="https://github.com/bialet/bialet/stargazers"><img src="https://img.shields.io/github/stars/bialet/bialet?style=social" alt="Stars"></a>
</p>

```wren
var users = `SELECT id, name FROM users`.fetch
var title = "🗂️ Users list"

return <!doctype html>
  <html>
    <head><title>{{ title }}</title></head>
    <body style="font: 1.5em/2.5 system-ui; text-align:center">
      <h1>{{ title }}</h1>
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
```

Bialet is a single self-contained binary (under 1 MB) that embeds the object-oriented [Wren language](https://wren.io), an HTTP server, and a built-in SQLite database. Requests are routed from the filesystem: each `.wren` file is a handler that returns a response body. It is written in C17 and runs on Linux and macOS (and Windows via cross-compilation).

## Install

Install a prebuilt binary with a single command (macOS ARM, Ubuntu x86_64, Ubuntu ARM):

```bash
curl -fsSL https://get.bialet.dev | sh
```

## Quickstart

1. Create an `index.wren` file in your app directory and start the server:

```bash
bialet
```

2. Visit [127.0.0.1:7001](http://127.0.0.1:7001) in your browser.

## Build from source

### Prerequisites

- A C17-compatible compiler (`gcc` or `clang`) and `make`
- `git`
- Development headers for SQLite, libcurl, and OpenSSL (see below)
- `python3` — only needed to regenerate the embedded Wren sources (`make wren_files`)

### Dependencies

**Debian/Ubuntu**

```bash
sudo apt install -y build-essential libsqlite3-dev libcurl4-openssl-dev libssl-dev
```

**macOS**

```bash
brew install sqlite3 curl openssl pkg-config
```

**Windows** — Bialet is cross-compiled from Linux with MinGW (`CC=x86_64-w64-mingw32-gcc make`). The resulting binary needs these DLLs alongside it at runtime:

- `libsqlite3-0.dll`
- `libcrypto-3-x64.dll`
- `libssl-3-x64.dll`

OpenSSL is optional but recommended for production (enables TLS). The build auto-detects it and defines `HAVE_SSL` when present; on macOS it is located via `pkg-config`.

### Compile and install

```bash
git clone https://github.com/bialet/bialet.git
cd bialet
make               # compiles to ./build/bialet
make install       # copies the binary to ~/.local/bin
```

The default build uses `-Wall -Wextra -Werror` and links against `libsqlite3`, `libcurl`, `libpthread`, and `libm` (plus `libssl`/`libcrypto` when OpenSSL is detected).

### Make targets

| Target             | Description                                                        |
| ------------------ | ------------------------------------------------------------------ |
| `make` / `make all`| Build the binary to `./build/bialet`                               |
| `make install`     | Copy the binary to `~/.local/bin`                                  |
| `make uninstall`   | Remove the installed binary                                        |
| `make check`       | Build and run the test suite                                       |
| `make installcheck`| Install, then run the tests against the installed binary           |
| `make static`      | Linux only — produce a statically linked, self-contained binary    |
| `make wren_files`  | Regenerate `src/*.wren.inc` from `src/*.wren` (run after editing embedded Wren sources) |
| `make html`        | Build the documentation with Sphinx                                |
| `make clean`       | Remove `build/` and test databases                                 |

## Development

Run the freshly built binary against an app directory (the last positional argument is the app root, defaulting to the current directory):

```bash
make                        # rebuild after editing C sources
./build/bialet /path/to/dev-app
```

If you change any embedded `.wren` source under `src/`, regenerate the C string includes before rebuilding:

```bash
make wren_files && make
```

### Command-line options

```
Usage: bialet [-h host] [-p port] [-l log] [-d database] [-t file] [-T] root_dir
```

| Option        | Description                                          |
| ------------- | ---------------------------------------------------- |
| `-h host`     | Host to bind (default `127.0.0.1`)                   |
| `-p port`     | Port to listen on                                    |
| `-l log`      | Write logs to a file (disables colored output)       |
| `-d database` | SQLite database file (default `_db.sqlite3`)         |
| `-w`          | Enable SQLite WAL mode                               |
| `-i files`    | Glob of files to ignore                              |
| `-m` / `-M`   | Memory soft / hard limit in MB                       |
| `-c` / `-C`   | CPU soft / hard limit in seconds                     |
| `-r code`     | Run an inline Wren snippet and exit                  |
| `-t file`     | Validate the syntax of a `.wren` file                |
| `-T [dir]`    | Run the test suite                                   |
| `-v`          | Print the version and exit                           |
| `root_dir`    | App directory to serve (default `.`)                 |

## Documentation

Full documentation at [bialet.dev](https://bialet.dev):

- [Getting Started](https://bialet.dev/getting-started.html) — Build a poll app from scratch
- [Installation](https://bialet.dev/installation.html) — Script, Homebrew, Docker, or source
- [Database](https://bialet.dev/database.html) — Queries, migrations, and mappings
- [API Reference](https://bialet.dev/reference.html) — Complete class documentation
- [FAQ](https://bialet.dev/faq.html) — Common questions and answers

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development setup and guidelines.

## Roadmap

See [ROADMAP.md](ROADMAP.md) for planned features and improvements.

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
