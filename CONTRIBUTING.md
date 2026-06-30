# Contributing to Bialet

Thanks for your interest in contributing. Bialet is a single-binary web
framework written in C that embeds a modified Wren VM and SQLite. This
guide covers how to set up, build, test, and submit changes.

## Code of Conduct

Be respectful. Keep discussions constructive. Assume good intent.

## Getting Started

### Prerequisites

**Linux (Debian/Ubuntu):**

```bash
sudo apt install -y build-essential libsqlite3-dev libssl-dev
# Optional, recommended for production:
sudo apt install -y libcurl4-openssl-dev
```

**macOS:**

```bash
brew install sqlite3 curl
# Optional, recommended for production:
brew install openssl
```

### Build

```bash
make clean && make
```

The binary is at `build/bialet`.

### Run Tests

```bash
# Full test suite (integration + built-in tests)
make check

# Integration tests only
./tests/run.sh ./build/bialet 127.0.0.1 7111

# Built-in test framework only
./build/bialet -T tests/
```

### Run in Development Mode

```bash
make dev PATH_RUN=/path/to/dev-app
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
2. Make your changes following the code style guidelines below
3. Add tests if applicable
4. Run `make` and `make check` to validate
5. Submit a pull request

## Code Style

### C Code

- Follow the existing style (`.clang-format` in the root — LLVM-based,
  2-space indent, 85-char column limit)
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
