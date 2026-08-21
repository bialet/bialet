# Contributing to Bialet

Thanks for your interest in contributing. Bialet is a single-binary web
framework written in C that embeds a modified Wren VM and SQLite. This
guide covers how to set up, build, test, and submit changes.

## Code of Conduct

Be respectful. Keep discussions constructive. Assume good intent.

## Getting Started

### Prerequisites

- A C17-compatible compiler (`gcc` or `clang`) and `make`
- `git`
- Development headers for SQLite, libcurl, and OpenSSL (see below)
- `python3` — only needed to regenerate the embedded Wren sources
  (`make wren_files`)

**Linux:**

Package names vary by distribution, but the set is always the same: a C17
compiler (`gcc`/`clang`), `make`, and development headers for SQLite, libcurl,
and OpenSSL. OpenSSL is optional but recommended (enables TLS); the build
auto-detects it and links it in when present.

**Debian / Ubuntu / Linux Mint / Pop!\_OS / Kali (apt):**

```bash
sudo apt install -y build-essential libsqlite3-dev libcurl4-openssl-dev libssl-dev
```

**Fedora / RHEL / CentOS Stream / Rocky Linux / AlmaLinux (dnf):**

```bash
sudo dnf install -y gcc make sqlite-devel libcurl-devel openssl-devel
```

**Arch Linux / Manjaro / EndeavourOS (pacman):**

```bash
sudo pacman -S --needed base-devel sqlite curl openssl
```

`base-devel` provides the compiler and `make`; the SQLite and libcurl dev
headers ship inside the regular `sqlite` and `curl` packages.

**openSUSE (zypper):**

```bash
sudo zypper install -y gcc make sqlite3-devel libcurl-devel libopenssl-devel
```

**Alpine Linux (apk):**

```bash
sudo apk add build-base sqlite-dev curl-dev openssl-dev
```

**Void Linux (xbps):**

```bash
sudo xbps-install -y base-devel sqlite-devel libcurl-devel openssl-devel
```

**NixOS (nix):**

```bash
nix-shell -p gcc sqlite curl openssl
```

**Gentoo (emerge):**

`gcc` and `make` are part of the base toolchain on Gentoo and usually already
installed. Add the libraries:

```bash
sudo emerge --ask dev-db/sqlite net-misc/curl dev-libs/openssl
```

**macOS:**

```bash
brew install sqlite3 curl openssl pkg-config
```

**Windows** — Bialet is cross-compiled from Linux with MinGW (see
[Cross-compiling](#cross-compiling) below). The resulting binary needs these
DLLs alongside it at runtime:

- `libsqlite3-0.dll`
- `libcrypto-3-x64.dll`
- `libssl-3-x64.dll`

OpenSSL is optional but recommended for production (enables TLS). The build
auto-detects it and defines `HAVE_SSL` when present; on macOS it is located
via `pkg-config`.

### Build

```bash
git clone https://github.com/bialet/bialet.git
cd bialet
make               # compiles to ./build/bialet
make install       # copies the binary to ~/.local/bin
```

The default build uses `-Wall -Wextra -Werror` and links against
`libsqlite3`, `libcurl`, `libpthread`, and `libm` (plus `libssl`/`libcrypto`
when OpenSSL is detected).

#### Make targets

| Target              | Description                                                                             |
| ------------------- | ----------------------------------------------------------------------------------------------- |
| `make` / `make all` | Build the binary to `./build/bialet`                                                    |
| `make install`      | Copy the binary to `~/.local/bin`                                                       |
| `make uninstall`    | Remove the installed binary                                                             |
| `make check`        | Build and run the test suite                                                            |
| `make installcheck` | Install, then run the tests against the installed binary                                |
| `make static`       | Linux only — produce a statically linked, self-contained binary                         |
| `make wren_files`   | Regenerate `src/*.wren.inc` from `src/*.wren` (run after editing embedded Wren sources) |
| `make html`         | Build the documentation with Sphinx                                                     |
| `make clean`        | Remove `build/` and test databases                                                      |

### Cross-compiling

Docker-based, one-shot (no local MinGW setup needed):

```bash
./tools/crosscompile.sh windows   # or: linux | all
```

This cross-builds static SQLite and OpenSSL for the `x86_64-w64-mingw32`
target inside a container, then runs `CC=x86_64-w64-mingw32-gcc make
static`. The resulting binary is copied to `build/bialet-windows-x86_64.exe`.

If you already have a MinGW cross-toolchain and SQLite/OpenSSL built for
`x86_64-w64-mingw32` on your machine, you can skip Docker:

```bash
CC=x86_64-w64-mingw32-gcc make static
```

See `tools/crosscompile.sh`'s `build_windows()` function for the exact
dependency-build steps if you need to set these up manually.
`.github/workflows/release.yml` runs the same process for releases.

### Run Tests

```bash
# Full test suite (integration + built-in tests)
make check

# Integration tests only
./tests/run.sh ./build/bialet 127.0.0.1 7111

# Built-in test framework only
./build/bialet -T tests/
```

By default, both suites print a colored line per test (✓ passed, ✗ failed,
○ skipped) plus a summary. Pass `-q` for quiet/CI-friendly output instead: no
colors, no per-test lines — just one summary line and a `### FAIL` block per
failure:

```bash
./tests/run.sh -q ./build/bialet 127.0.0.1 7111
./build/bialet -T tests/ -q
```

```
$ ./build/bialet -T tests/ -q
1 of 14 tests failed in 110ms

### FAIL _tests/login.wren:37 - login
Expected code to be 401 but was 500
```

### Testing Against a Remote Server

`tests/run.sh` can run the integration suite against a bialet instance
running elsewhere — useful for validating a cross-compiled binary (e.g. the
Windows `.exe`) without running it locally. Pass `-` as the executable to
skip spawning a local process:

```bash
./tests/run.sh - <remote-host> <port> <echo-port>
```

To set this up:

1. Copy the binary and the `tests/` directory to the target machine.
2. On that machine, start two instances the way `run.sh` would locally: one
   serving `tests/` on the main port, one serving `tests/echo` on the echo
   port.
3. From your dev machine, run the command above pointing at that host.

Two things are auto-skipped in this mode: syntax validation (`-t`) and
dev-mode tests (both need local binary access), and the symlink/FS-sharing
regression test (skipped unless the remote server can see the same
filesystem as the test runner).

### Run in Development Mode

```bash
make dev PATH_RUN=/path/to/dev-app
```

Or run the freshly built binary directly against an app directory:

```bash
./build/bialet /path/to/dev-app
```

If you changed any embedded `.wren` source under `src/`, regenerate the C
string includes before rebuilding:

```bash
make wren_files && make
```

## Project Structure

```
src/       — C runtime, Wren VM, HTTP server, SQLite bindings
docs/      — User-facing documentation (Sphinx + MyST)
tests/     — Integration tests (shell runner + .wren test files)
tools/     — Build helpers (wren_to_c_string.py, cross-compile)
```

Key C entrypoints:
- `src/main.c` — Process lifecycle, reload, cron, migrations
- `src/server.c` — HTTP dispatch, path resolution

## Development Workflow

1. Fork the repository and create a feature branch
2. Enable the pre-commit hook: `make install-hooks`
3. Make your changes following the naming and style guidelines below
4. Add tests if applicable
5. Run `make` and `make check` to validate
6. Submit a pull request

The pre-commit hook (`.githooks/pre-commit`) validates clang-format on staged
C/H files and runs `make check` on every commit. Skip it selectively with
`SKIP_CLANG_FORMAT=1` and/or `SKIP_BIALET_TESTS=1`.

## Code Style

### C Code

Bialet enforces two standards on C code: **naming conventions** and
**clang-format** (the `.clang-format` file in the repo root). Both are checked
automatically by the pre-commit hook.

#### Naming Conventions

All user-defined identifiers follow snake_case. This applies to functions,
global variables, and static globals. It does not apply to:

- Standard C library and POSIX functions (`printf`, `fopen`, `pthread_create`)
- Macros (`#define`) and enum constants (these are `UPPER_CASE`)
- Strings, comments, and Wren-side identifiers
- Vendored third-party code (`wren_*`, `dmon.h`, `getopt.c`, `getopt.h`,
  `favicon.h`)

**Function names:**

- snake_case (lowercase with underscores) — CamelCase is prohibited
- Verb-first ordering (action then object): `install_cron()`,
  `trigger_reload_files()`, `start_server()`, `create_bialet_query()`
- Module-prefix names are fine and stay put: `livereload_init()`,
  `bialet_run()`, `server_poll()`

**Variables:**

- Global and static-global variables are snake_case
- Struct/typedef names keep their existing style (e.g. `BialetConfig`) — do not
  rename them
- Local variables keep their current names unless clearly poorly named

When you rename a function, update every definition, declaration in `src/*.h`,
and callsite in the same commit. The pre-commit hook enforces this
consistently.

#### clang-format

- Run `clang-format -i --style=file <files>` on every file you touch before
  committing
- Formatting follows `.clang-format` (LLVM-based, 2-space indent, 85-char
  column limit, `PointerAlignment: Left`, `SpaceBeforeParens: Never`)
- The pre-commit hook validates staged C/H files with
  `clang-format --dry-run --Werror` and rejects unformatted commits
- Vendored files are excluded from validation: `wren_*`, `dmon.h`, `getopt.c`,
  `getopt.h`, `favicon.h`

#### Other C Rules

- Use `size_t` for lengths, not `int`
- Prefer `snprintf` over `sprintf`
- Check every `malloc`, `realloc`, `calloc`, and `strdup` for NULL
- Never pass user-controlled strings as format arguments
- Preserve public APIs in `src/*.h`
- Avoid reordering exported structs without adjusting consumers

See `AGENTS.md` for a full list of C security rules.

### Wren Code

- Follow existing patterns in `docs/examples/` and `tests/`
- Use the built-in Test class for test files
- Keep modules focused and single-purpose

### Documentation

- Documentation uses Sphinx with MyST (Markdown) parser
- `docs/requirements.txt` lists Python dependencies
- To build docs locally:

```bash
cd docs && pip install -r requirements.txt
# Install Pygments Wren lexer
cp lexer.py $(pip3 show pygments | grep Location | awk '{print $2}')/pygments/lexers/wren.py
# Build
sphinx-build -M html . ../build/
```

## CI/CD

Two GitHub Actions workflows run on push:
- **sphinx.yml** — Builds and deploys documentation to `bialet.dev` on push
  to `main`
- **release.yml** — Builds platform binaries (Linux x86_64, Linux ARM64,
  macOS ARM64) and creates a GitHub Release on version tags (`v*`)

## Submitting Changes

1. Ensure your branch is based on `main`
2. Keep PRs small and focused — one change per PR
3. Write a clear description of what changed and why
4. Link any related issues
5. CI must pass before merge

## Reporting Issues

- Search [existing issues](https://github.com/bialet/bialet/issues) first
- Include steps to reproduce, expected behavior, and actual behavior
- Include your OS and Bialet version (`bialet` with no args shows version)

## Questions?

Open a [GitHub Discussion](https://github.com/bialet/bialet/discussions)
or an issue with the question label.
