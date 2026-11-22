# Command Line Usage

`bialet` is a single binary. The command line controls how the server starts,
how it is configured, and a few standalone modes (`-r`, `-t`, `-T`).

## Starting the Server

Run `bialet` from your app directory:

```bash
bialet
```

This serves the current directory on `127.0.0.1:7001`.

### Development mode: `bialet dev`

```bash
bialet dev
```

Starts the server from the current directory, enables live reload and the
in-browser error display, and opens the server URL in your browser. Both are
on for that run only: nothing is written to the database, so a plain `bialet`
start afterwards (or on a copy of the database) has neither. Plain `bialet`
does not open the browser.

> On Windows, double-clicking `bialet.exe` in File Explorer counts as a dev
> launch: the server starts in dev mode with the browser opening automatically.
> Run it from a terminal to start without dev mode.

See [Live Reload](live-reload.md) and [Error Pages](errors.md) for what dev
mode turns on.

## Options

Pass the app directory and flags to control startup:

```bash
bialet -p 8080 -h 0.0.0.0 /path/to/app
```

Every option has a short and a long form. Values can be passed as
`--option value` or `--option=value`:

```bash
bialet --port 8080 --host 0.0.0.0 /path/to/app
bialet --port=8080 /path/to/app
```

| Parameter               | Description                                                                 | Default                                      |
| ----------------------- | --------------------------------------------------------------------------- | -------------------------------------------- |
| `-p`, `--port`          | Port number                                                                 | `7001`                                       |
| `-h`, `--host`          | Host name                                                                   | `127.0.0.1`                                  |
| `-H`, `--help`          | Show this help and exit                                                     | None                                         |
| `-r`, `--run`           | Run the code passed as argument, then exit                                  | None                                         |
| `-t`, `--validate`      | Compile a Wren file and its local imports, without executing them, then exit | None                                        |
| `-T`, `--tests`         | Run tests in the `_tests/` folder                                           | None                                         |
| `-v`, `--version`       | Print the version and exit                                                  | None                                         |
| `-l`, `--log`           | Log file location                                                           | `stdout`                                     |
| `-d`, `--db`            | SQLite database file location                                               | `_db.sqlite3`                                |
| `-w`, `--wal`           | Enable SQLite [Write-Ahead logging mode](https://www.sqlite.org/wal.html)   | Disabled                                     |
| `-i`, `--ignore`        | Ignored files: comma-separated globs; replaces the default list             | `README*,AGENTS*,LICENSE*,*.json,*.yml,*.yaml,*.exe` |
| `-m`, `--mem-soft`      | Memory soft limit (MB)                                                      | `128`                                        |
| `-M`, `--mem-hard`      | Memory hard limit (MB)                                                      | `256`                                        |
| `-c`, `--cpu-soft`      | CPU time soft limit (seconds)                                               | `3600`                                       |
| `-C`, `--cpu-hard`      | CPU time hard limit (seconds)                                               | `7200`                                       |
| `-b`, `--max-post`      | Max request body (KB)                                                       | `128`                                        |
| `-u`, `--max-upload-size` | Max per-file upload (bytes; `K`/`M`/`G` suffix scales it, e.g. `4MB`)     | `4 MB` (see note below)                      |
| `-f`, `--foreign-keys`  | SQLite foreign keys: `on` or `off`                                          | `on`                                         |
| `-s`, `--synchronous`   | SQLite synchronous: `off`, `normal`, `full`, or `extra`                     | `normal`                                     |
| `-k`, `--secret-key-file` | AES-256 key for `Config.encrypt` / `Config.decrypt`: 64 hex chars or a passphrase, outside the app root | None |
| `-q`, `--quiet`         | Quiet: suppress the browser auto-open and colored output                    | Disabled                                     |
| `--no-watch`            | Disable file watching: no live reload, and migrations/cron are not re-run when their file is saved | Disabled (watcher on) |

Long options that require a value reject an empty one (`--port` alone is an
error). Unknown options, short or long, are rejected with an error and the
usage text instead of being silently ignored.

> ⚠️ Pitfall: bialet refuses to start when the port already has a listener, even
> when the addresses differ (`127.0.0.1` vs `0.0.0.0`). The kernel would accept
> that second bind — `SO_REUSEADDR` allows a wildcard bind to share a port with a
> specific address — and then route loopback traffic to the older server, so the
> new one would never answer on `127.0.0.1`. Stop the other process
> (`lsof -nP -iTCP:7001 -sTCP:LISTEN`) or pick another port.

> ⚠️ Pitfall: `-d` and `-l` may point inside the app root only under a private
> name (starting with `_` or `.`, like `_db.sqlite3`). Everything else below the
> root is served, so a database or a log file with a public name there is
> downloadable by any client. Bialet refuses to start and exits with status 11.
> The public `.well-known/` directory counts as public here too.

> ⚠️ Pitfall: the `-l` file records request targets, query strings included,
> so Bialet creates it readable by its owner only (`0600`). An existing file
> keeps its mode; do not make it group- or world-readable.

> ⚠️ Pitfall: the `-l` file is only ever appended to. Every request adds one
> line (the method and the first 512 bytes of the target), so rotate it with
> the system's tools, e.g. logrotate with `copytruncate` (Bialet keeps the file
> open in append mode, so truncating it in place is safe). Under systemd or
> Docker, logging to stdout and letting journald or the log driver cap the size
> works too.

Size values for `-u` are bytes; a `K`, `M`, or `G` suffix (KiB/MiB/GiB) scales
them. `-u 4MB`, `-u 4096KB`, and `-u 4194304` are the same limit.

`-m` and `-M` set the address-space limit (`RLIMIT_AS`) of the HTTP process on
Linux. A soft limit above the hard one is lowered to it, a hard limit above the
one the process inherited (systemd `LimitAS=`, `ulimit -v`) is lowered to that,
and `0` means no limit of its own. macOS and Windows do not enforce them: there
the values only size the request-body cap below.

`-c` and `-C` are **seconds of CPU time** for the HTTP process, not a
percentage. The supervisor runs the server as a parent/child pair: reaching the
soft limit sends `SIGXCPU`, reaching the hard limit sends `SIGKILL`, and in
either case the parent starts a fresh child, which drops the compiled-bytecode
cache and every open connection. The defaults (one and two hours) are sized so
that only a runaway process hits them; lower them only to bound a specific
workload. `0` means no limit, with the shell's own `ulimit -t` still applying
as the ceiling.

> The upload cap is also bounded by the request-body limit (`-b`). A file must
> fit inside a multipart body smaller than `-b` (default 128 KB), so the
> effective per-file limit is the smaller of `-u` and that body cap minus ~8 KB
> of multipart framing. With defaults that is about **120 KB**, not 4 MB. See
> [File Size Limits](file.md) for how to raise it.

### Version

Print the installed version and exit:

```bash
bialet -v
# bialet {{bialet_version}}
```

### Quiet mode

`-q` (or `--quiet`) suppresses the browser auto-open and colored log output.
Useful in scripts, containers, and CI:

```bash
bialet -q /path/to/app
```

### Editing migrations without applying them

A migration runs at startup and again whenever `_migration.wren` is saved. While
you draft one, that save-triggered run applies half-written SQL to the
database. Start the server with `--no-watch` to keep only the startup run:

```bash
bialet --no-watch /path/to/app
```

Edit and save the file freely, then check that it compiles without running it:

```bash
bialet -t _migration.wren
```

Restart the server without `--no-watch` to apply the new migration. The flag
turns the whole file watcher off, so live reload is off for that run too. The
server still runs migrations and installs cron once at startup, and prints a
notice saying watching is disabled.

## Run Code from the Command Line

Use `-r` to run Wren code once and exit:

```bash
bialet -r 'System.log("Hello, World!")'
```

The response will be printed directly.

```bash
bialet -r 'return "No log, plain response"'
```

You have to respect newlines in the code.

The script runs without a request: `Request.method`, `Request.get(...)` and
`Request.post(...)` return null. It does initialize the framework tables, so
`Config` and `Session` work. Your app's own tables exist only if the database
was already migrated (for example by starting the server or running `bialet -T`).

Relative imports resolve against the app root, so a module next to your pages
can be imported directly:

```bash
bialet -r 'import "lib" for X
System.print(X.value)'
```

The import still goes through the module loader's containment check: a path that
escapes the app root (`import "../outside"`) fails and exits non-zero. Wren uses
newlines, not semicolons, so separate statements with a newline.

## Running Tests

Bialet includes a built-in testing framework. See [Testing Guide](tests.md)
for full documentation.

```bash
bialet -T                    # Run all tests in _tests/
bialet -T docs/examples      # Run tests in a specific directory
```

## Validate Syntax of a Wren File

To check the syntax of a Wren file without executing it, use `-t`. Run it the
same way you run the server:

If you start your server from the app directory:

```bash
cd /path/to/app
bialet               # starts server
bialet -t index.wren # compiles the file
```

If you start your server with an explicit root path:

```bash
bialet /path/to/app                         # starts server
bialet -t /path/to/app/index.wren /path/to/app # compiles the file
```

The root path is required when your file imports from `_app/` or uses relative
imports. If you omit it for such files, the validator will exit with an error.

`-t` compiles the file and every local module it imports. It does not execute
any of them, so runtime errors are not reported and migrations do not run:
`bialet -t _migration.wren` leaves the database untouched even when the
migration is valid. Remote imports (`http:`, `gh:`) are not fetched. A missing
local import is not a syntax error; it only fails when the page runs.

This will exit with code `0` if the syntax is valid, or code `1` if there are
compilation errors. Useful for CI/CD pipelines and pre-commit hooks.

```bash
# Example: Check syntax before deploying (explicit root path)
if bialet -t /path/to/app/main.wren /path/to/app; then
    echo "Syntax OK, deploying..."
else
    echo "Syntax errors found, aborting deployment"
    exit 1
fi
```

## Upload and SQLite Tuning

These limits and SQLite pragmas are startup flags, set per app per launch:

### File Upload Limit

- **Max Upload Size** (`-u`, `--max-upload-size`): the per-file cap for
  `Request.file()`. Files over it are rejected and logged with the limit.
  Default 4 MB, and bounded by the request-body cap — see
  [File Size Limits](file.md). Example: `bialet -u 8MB /path/to/app`.

### SQLite Pragma Settings

- **Foreign Keys** (`-f`, `--foreign-keys`): enable or disable foreign key
  constraints (default: `on`). Disable with `-f off` only when you know a
  migration depends on it; otherwise orphaned rows go unchecked.
- **Synchronous** (`-s`, `--synchronous`): controls how SQLite writes to disk
  (default: `normal`).
  - `off`: Fastest, least safe
  - `normal`: Balanced performance and safety (default)
  - `full`: Very safe, slower
  - `extra`: Maximum safety, slowest

```bash
bialet -f off -s full -u 2MB /path/to/app
```

SQLite applies these at connection open, so changing them requires restarting
the server.
