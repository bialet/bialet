# Bialet Development Guide

Bialet is a single-process C web framework embedding a heavily modified Wren
language and SQLite.

## Project Structure

- `src/` - C runtime & Wren glue
- `docs/` - User-facing documentation
- `docs/examples/` - App examples
- `tests/` - Integration tests

## Quick Commands

```bash
# Build
make

# Run tests
./tests/run.sh ./build/bialet 127.0.0.1 7111

# Alternative: make check (calls tests/run.sh)
make check

# Install (copies build/bialet to ~/.local/bin)
make install

# Clean build
make clean && make
```

## Runtime & Development Notes

### C Entrypoints

- `src/main.c` - Process lifecycle, reloads, cron, migrations
- `src/server.c` - HTTP dispatch, path resolution

### Special Wren Files

Wren app files are plain `.wren` files served from a root app directory:

- `_app.wren` - Site template / app namespace
- `_route.wren` - Per-directory route handlers (server searches upward), used
  only when SEO friendly URLs is a must. Prefer GET parameters otherwise.
- `_migration.wren` or `/_app/migration.wren` - DB migrations (run at start)
- `/_cron.wren` or `/_app/cron.wren` - Optional cron tasks
- `_db.sqlite3` - SQLite DB file for the app

### Security & Routing

Files starting with `_` or `.` are forbidden (server returns 403). The server
prefers `.wren` handlers, falls back to `.html`, and attempts directory index
files.

## Bialet App Patterns

- Think of Bialet apps as classic multi-page web apps, not single-page apps.
  Server renders full pages; use forms and links for navigation.
- Inline HTML: `{{ ... }}` interpolation inside `<...>` blocks
- Queries: Use backticks `` `SELECT ...` `` for prepared Query objects with `?`
  placeholders
- Request handling: `Request.isPost`, `Request.post(name)`, `return` for response body,
  `Response.redirect(path)`
- DB values come back as strings - convert with `Num.fromString(...)` before
  numeric math or use `query.num` for direct numeric retrieval.

## Build & Link Notes

- `Makefile` uses `-lsqlite3`, `-lcurl`, and conditionally links OpenSSL
- Changes touching networking or DB need matching LDFLAGS/CFLAGS updates
- Wren sources embedded as C strings via `tools/wren_to_c_string.py` and
  `wren_*.wren.inc` files

## Tests & CI

- `tests/run.sh` drives integration checks by calling the built binary and
  asserting HTTP responses
- Documentation built with Sphinx (`docs/`); workflow in
  `.github/workflows/sphinx.yml`

## Debugging

- Use `System.print(...)` inside Wren files for runtime logs on server stdout
- Live-reload uses inotify on Linux to watch `.wren` files (see `src/main.c`)
- On macOS, restart the server manually for changes

## C Editing Guidelines

- Preserve public APIs in `src/*.h`
- Avoid reordering exported struct/layout changes without adjusting consumers
- Prefer small, focused changes
- Run `make` then `make check` to validate

## C Security Rules

These rules prevent the most common and severe C vulnerabilities. Violations
will be flagged during code review.

### 1. Null-terminate network buffers before string ops

`recv()` does NOT null-terminate. Always read `sizeof(buf) - 1` bytes and
write `'\0'` after the last byte before passing the buffer to `strstr()`,
`strlen()`, `printf()`, or any function expecting a C string.

```c
// Correct
ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
if (n > 0) buf[n] = '\0';
```

### 2. Never use fixed-size buffers with dynamic content

If the size depends on caller input (headers, log messages, file paths), compute
the required size at runtime. Use `snprintf(NULL, 0, fmt, ...)` to measure, then
`malloc(needed + 1)` to allocate.

```c
// Correct — dynamic allocation
int needed = snprintf(NULL, 0, "HTTP/1.1 %d %s\r\n%s\r\n", status, desc, headers);
char *msg = malloc(needed + 1);
snprintf(msg, needed + 1, "HTTP/1.1 %d %s\r\n%s\r\n", status, desc, headers);
```

### 3. No large stack allocations

Never allocate unbounded arrays on the stack. Arrays > 4096 bytes risk stack
overflow, especially in recursive or nested call paths. Use `malloc`/`realloc`
for any array whose size is not trivially bounded at compile time.

```c
// Wrong — 80KB on stack, no bounds check
char *lines[10000];

// Correct — heap-allocated with growth
char **lines = malloc(capacity * sizeof(char *));
// ... realloc() as needed, free() at end
```

### 4. Check `ftell()` and `fseek()` return values

`ftell()` returns `-1` on error. Passing `-1` to `malloc()` or `fread()` causes
undefined behavior (typically a massive overflow). Always check before use.

```c
// Correct
long len = ftell(f);
if (len < 0) { fclose(f); return NULL; }
buffer = malloc(len + 1);
```

### 5. Validate filesystem paths with `realpath()`

Any user-controlled path must be resolved with `realpath()` and verified to be
within the allowed root directory. This prevents directory traversal attacks.

```c
// Correct
char resolved[PATH_MAX];
if (realpath(user_path, resolved) == NULL) return ERROR;
if (strncmp(resolved, root_dir, root_len) != 0) return ERROR;
// safe to use resolved now
```

### 6. Use `size_t` for lengths, not `int`

`int` overflows at 2GB on 64-bit systems and silently wraps to negative on
`len * 2` expressions. Always use `size_t` for buffer sizes, string lengths,
and allocation arithmetic.

```c
// Wrong
int len = strlen(s);
char *buf = malloc(len * 2 + 1);  // overflow possible

// Correct
size_t len = strlen(s);
char *buf = malloc(len * 2 + 1);
```

### 7. `trim()` must modify the buffer in-place

Advancing a local pointer without shifting content means callers still see the
original (untrimmed) string. Use `memmove()` to shift the trimmed portion to the
start of the buffer.

### 8. Prefer `snprintf` over `sprintf`

`sprintf` has no bounds checking. Always use `snprintf` with the actual buffer
size — never assume the output fits.

### 9. Check every allocation

Every `malloc`, `realloc`, `calloc`, and `strdup` call must have a NULL check.
Do not just `exit()` — clean up resources (close FDs, free partial allocations)
before returning an error.

### 10. Avoid format string vulnerabilities

Never pass user-controlled strings as the format argument to `printf`/`fprintf`/
`snprintf`. Use `"%s"` as the format and pass the string as an argument.
