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
in-browser error display, and opens the server URL in your browser. The flags
are stored in the database and persist across restarts, so you only need to
run `dev` once. Plain `bialet` starts the server without touching those flags
or opening the browser.

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
| `-t`, `--validate`      | Validate the syntax of a Wren file, then exit                               | None                                         |
| `-T`, `--tests`         | Run tests in the `_tests/` folder                                           | None                                         |
| `-v`, `--version`       | Print the version and exit                                                  | None                                         |
| `-l`, `--log`           | Log file location                                                           | `stdout`                                     |
| `-d`, `--db`            | SQLite database file location                                               | `_db.sqlite3`                                |
| `-w`, `--wal`           | Enable SQLite [Write-Ahead logging mode](https://www.sqlite.org/wal.html)   | Disabled                                     |
| `-i`, `--ignore`        | Ignored files: comma-separated list of glob expressions                     | `README*,AGENTS*,LICENSE*,*.json,*.yml,*.yaml` |
| `-m`, `--mem-soft`      | Memory soft limit (MB)                                                      | `128`                                        |
| `-M`, `--mem-hard`      | Memory hard limit (MB)                                                      | `256`                                        |
| `-c`, `--cpu-soft`      | CPU soft limit (%)                                                          | `15`                                         |
| `-C`, `--cpu-hard`      | CPU hard limit (%)                                                          | `30`                                         |
| `-b`, `--max-post`      | Max request body (KB)                                                       | `128`                                        |
| `-q`, `--quiet`         | Quiet: suppress the browser auto-open and colored output                    | Disabled                                     |

Long options that require a value reject an empty one (`--port` alone is an
error). Unknown options, short or long, are rejected with an error and the
usage text instead of being silently ignored.

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
bialet -t index.wren # validates syntax
```

If you start your server with an explicit root path:

```bash
bialet /path/to/app                         # starts server
bialet -t /path/to/app/index.wren /path/to/app # validates syntax
```

The root path is required when your file imports from `_app/` or uses relative
imports. If you omit it for such files, the validator will exit with an error.

This will validate the syntax and exit with code `0` if the syntax is valid, or
code `1` if there are compilation errors. Useful for CI/CD pipelines and
pre-commit hooks.

```bash
# Example: Check syntax before deploying (explicit root path)
if bialet -t /path/to/app/main.wren /path/to/app; then
    echo "Syntax OK, deploying..."
else
    echo "Syntax errors found, aborting deployment"
    exit 1
fi
```

## Advanced Configuration

Bialet provides several advanced configuration options that can be set
programmatically in the C code (future versions may expose these as CLI
parameters):

### File Upload Limits

- **Max Upload Size**: Controls the maximum file size for uploads (default: 10
  MB)
- Files exceeding this limit will be rejected with an error message

### SQLite Pragma Settings

- **Foreign Keys**: Enable/disable foreign key constraints (default: ON)
- **Synchronous Mode**: Controls how SQLite writes to disk (default: NORMAL)
  - OFF: Fastest, least safe
  - NORMAL: Balanced performance and safety (default)
  - FULL: Very safe, slower
  - EXTRA: Maximum safety, slowest

These settings are optimized for most use cases but can be adjusted in the
source code if needed.
