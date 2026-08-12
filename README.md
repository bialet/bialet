# 🚲 [Bialet](https://bialet.dev)

<p align="center">
  <a href="https://bialet.dev">
    <img src="docs/_static/logo.png" alt="" width="200" />
  </a>
</p>
<p align="center">
  <strong>Web development became a spaceship. Bialet is a bicycle.</strong>
</p>
<p align="center">
  Build data-driven web apps from a single file. No NPM, no YAML, no separate<br />
  database servers. Just a <strong>tiny binary</strong> with a built-in HTTP server,<br />
  a lightweight scripting language, and SQLite.
</p>

<p align="center">
  <a href="https://github.com/bialet/bialet/releases">
      <img src="https://img.shields.io/github/v/release/bialet/bialet?color=%237c3aed&label=version" alt="Version">
  </a>
  <a href="https://github.com/bialet/bialet/blob/main/LICENSE">
      <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License">
  </a>
  <a href="https://github.com/bialet/bialet/stargazers">
      <img src="https://img.shields.io/github/stars/bialet/bialet?style=social" alt="Stars">
  </a>
</p>

A script that stays out of your way:

```wren
// 1. Create your database table automatically
`CREATE TABLE IF NOT EXISTS messages (text TEXT)`.query

// 2. Handle POST requests and save data
if (Request.isPost) {
  var msg = Request.post("msg") || ""
  `messages`.save({"text": msg})
}

// 3. Fetch data using pure SQL
var messages = `SELECT * FROM messages`.fetch

// 4. Return HTML directly
return <main>
  <h1>Bialet Guestbook</h1>

  <form method="post">
    <input name="msg" placeholder="Write a message...">
    <button>Submit</button>
  </form>
  <hr>
  <ul>
    {{ messages.map {|m| <li>{{ m["text"] }}</li> } }}
  </ul>
</main>
```

Bialet integrates the object-oriented [Wren language](https://wren.io) with an
HTTP server and a built-in SQLite database — **in a single small binary**, with
zero configuration files, zero dependencies, and no build step. Requests are
routed from the filesystem: each `.wren` file is a handler that returns a
response body. It is written in C17 and runs on Linux and macOS (and Windows via
cross-compilation).

## Install

Install a prebuilt binary with a single command (macOS ARM, Ubuntu x86_64,
Ubuntu ARM):

```bash
curl -fsSL https://get.bialet.dev | sh
```

Check for [other installation options](https://bialet.dev/installation.html).

## Quickstart

Learn how to build a simple poll app at the
[getting started documentation](https://bialet.dev/getting-started.html).

### Linux or macOS

```bash
cd /path/to/project
bialet dev
```

### Windows

Copy the `bialet.exe` file on your project folder and double click it.

Double-clicking `bialet.exe` in File Explorer starts it in dev mode: live
reload, in-browser error display, and the browser opens automatically. Run it
from a terminal to start without dev mode.

## The Bialet Manifesto — Ride Light 🚲

1. **Simplicity is a superpower.** Every line of tooling you don't write is a
   line of your app that ships faster.

2. **Standards, not frameworks.** HTML, SQL, and HTTP have outlived every
   framework. Master them and your knowledge stays relevant.

3. **One file to deploy.** No containers, no orchestration, no
   `docker-compose.yml`. Copy the binary — that's it.

4. **Batteries included.** Server, database, templating — all in one small
   binary. No external services to provision.

5. **Ride light.** Complexity is a choice. Choose less, and you'll go further
   than you think.

## Build from source

### Prerequisites

- A C17-compatible compiler (`gcc` or `clang`) and `make`
- `git`
- Development headers for SQLite, libcurl, and OpenSSL (see below)
- `python3` — only needed to regenerate the embedded Wren sources
  (`make wren_files`)

### Dependencies

**Debian/Ubuntu**

```bash
sudo apt install -y build-essential libsqlite3-dev libcurl4-openssl-dev libssl-dev
```

**macOS**

```bash
brew install sqlite3 curl openssl pkg-config
```

**Windows** — Bialet is cross-compiled from Linux with MinGW
(`CC=x86_64-w64-mingw32-gcc make`). The resulting binary needs these DLLs
alongside it at runtime:

- `libsqlite3-0.dll`
- `libcrypto-3-x64.dll`
- `libssl-3-x64.dll`

OpenSSL is optional but recommended for production (enables TLS). The build
auto-detects it and defines `HAVE_SSL` when present; on macOS it is located via
`pkg-config`.

### Compile and install

```bash
git clone https://github.com/bialet/bialet.git
cd bialet
make               # compiles to ./build/bialet
make install       # copies the binary to ~/.local/bin
```

The default build uses `-Wall -Wextra -Werror` and links against `libsqlite3`,
`libcurl`, `libpthread`, and `libm` (plus `libssl`/`libcrypto` when OpenSSL is
detected).

### Make targets

| Target              | Description                                                                             |
| ------------------- | --------------------------------------------------------------------------------------- |
| `make` / `make all` | Build the binary to `./build/bialet`                                                    |
| `make install`      | Copy the binary to `~/.local/bin`                                                       |
| `make uninstall`    | Remove the installed binary                                                             |
| `make check`        | Build and run the test suite                                                            |
| `make installcheck` | Install, then run the tests against the installed binary                                |
| `make static`       | Linux only — produce a statically linked, self-contained binary                         |
| `make wren_files`   | Regenerate `src/*.wren.inc` from `src/*.wren` (run after editing embedded Wren sources) |
| `make html`         | Build the documentation with Sphinx                                                     |
| `make clean`        | Remove `build/` and test databases                                                      |

## Development

Run the freshly built binary against an app directory (the last positional
argument is the app root, defaulting to the current directory):

```bash
make                        # rebuild after editing C sources
./build/bialet /path/to/dev-app
```

If you change any embedded `.wren` source under `src/`, regenerate the C string
includes before rebuilding:

```bash
make wren_files && make
```

### Command-line options

```
Usage: bialet [options] [dev] [root_dir]
```

Every option has a short and a long form; values accept `--option value` or
`--option=value`.

| Option                                  | Description                                    |
| --------------------------------------- | ---------------------------------------------- |
| `-h`, `--host`                          | Host to bind (default `127.0.0.1`)             |
| `-p`, `--port`                          | Port to listen on                              |
| `-l`, `--log`                           | Write logs to a file (disables colored output) |
| `-d`, `--db`                            | SQLite database file (default `_db.sqlite3`)   |
| `-w`, `--wal`                           | Enable SQLite WAL mode                         |
| `-i`, `--ignore`                        | Glob of files to ignore                        |
| `-m`, `--mem-soft` / `-M`, `--mem-hard` | Memory soft / hard limit in MB                 |
| `-c`, `--cpu-soft` / `-C`, `--cpu-hard` | CPU soft / hard limit in %                     |
| `-r`, `--run`                           | Run an inline Wren snippet and exit            |
| `-t`, `--validate`                      | Validate the syntax of a `.wren` file          |
| `-T`, `--tests`                         | Run the test suite                             |
| `-v`, `--version`                       | Print the version and exit                     |
| `-H`, `--help`                          | Print the help and exit                        |
| `dev`                                   | Live reload, error display, browser auto-open  |
| `root_dir`                              | App directory to serve (default `.`)           |

Read more on [the usage documentation](https://bialet.dev/usage.html).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development setup and guidelines.

## Roadmap

See [ROADMAP.md](ROADMAP.md) for planned features and improvements.

## License

Bialet is released under the MIT license, allowing users to freely use, modify,
and distribute the software with fewer restrictions.

## Credits

~I copy a lot of code from all over the web~ Bialet incorporates the work of
several open-source projects and contributors. We extend our gratitude to:

- The [Wren programming language](https://wren.io), for its lightweight,
  flexible, and high-performance capabilities.
- [PureFox48](https://github.com/PureFox48) for the upper and lower functions.
- [superwills](https://github.com/superwills) for providing the `getopt` source.
- DeepSeek v4 Flash & Pro, Gemini 3, Sonnet 5 and Opus 4.
- Coffee and mate cocido.

First versions were helped by:

- [Mongoose](https://mongoose.ws/) web server library.
- [Matthew Brandly](https://github.com/brandly/wren-json), for his invaluable
  contributions to JSON parsing and utility functions in Wren.
- [Codeium](https://github.com/codeium) for the [Codeium](https://codeium.com)
  plugin
- ChatGPT 3 & 3.5
- Even more coffee and mate cocido.

We encourage users to explore these projects and recognize the efforts of their
creators.
