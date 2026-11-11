# Changelog

Releases 1.2.6–1.2.13 were tagged and published on the
[releases page](https://github.com/bialet/bialet/releases); this file resumes at
1.2.14.

## 1.2.15 — 2026-11-11

### Security

- A route module is no longer returned as source text when its extension is
  requested in a different case. The interpreter-vs-static decision tested the
  *requested* basename with a case-sensitive compare while the file was opened
  through a case-insensitive filesystem, so `GET /blog.WREN` on macOS or Windows
  returned `blog.wren` to the client.
- `Request.login` decodes the Basic credential in linear time and rejects a
  malformed value with 401. It built its result by appending one byte at a time,
  so a 4 KB `Authorization` header cost about half a second of the single
  worker before the credential was even compared; a truncated value raised a
  runtime error instead of answering 401.
- The session table is bounded again. The write-path sweep was paced by a Wren
  class variable, which a new VM resets on every request, so neither the
  30-day expiry nor the `maxSessions` cap ever ran. The sweep now fires from a
  per-write draw (about one write in 62), `Session.rotate()` marks the id it
  mints, and the cap evicts only ids without that marker, so an anonymous flood
  can no longer log out real users. Both sweeps delete at most 1000 rows per
  pass.
- A session expires when the whole id has been idle for 30 days. The expiry
  tested individual rows, so a login-time row older than a month ended an
  actively used session, and one principal's logout deleted other sessions'
  login rows outright. `Session.destroy()` now deletes only the caller's id.
- An operator `-l` log file that resolves inside the app root at a public name
  is refused at startup, the same way `-d` already was. Every request line,
  query string included, was downloadable.
- The Markdown inline pass no longer rescans the rest of the line for every
  unmatched `[`, so a 128 KiB body cannot burn quarter-second chunks of the
  single worker.
- `Util.hash` / `Util.verify` hash and compare the whole credential. The C KDF
  re-derived the length with `strlen`, so a password containing a NUL byte was
  stored as its prefix and a credential starting with NUL became the empty
  password.

### Fixed

- Indexing a string at a byte sequence that decodes above U+10FFFF (for example
  `0xF4 0x90 0x80 0x80`) returns the raw byte, as range slicing already did,
  instead of an empty string that raised on `char.bytes[0]`. `wrenUtf8Decode`
  also rejects a zero-length span instead of reading past it.
- The `.wren.inc` generator embeds every source line verbatim. It dropped blank
  and `//`-prefixed lines without tracking string state, so a line inside a
  multi-line literal was deleted from the shipped program; removing the filter
  also makes the embedded text match the reviewed file line for line.

### Tests

- New coverage for each item above: case-variant routing, Basic-auth decoding,
  session cap and idle expiry, logout scope, Markdown linearity, the
  length-aware KDF, string indexing above U+10FFFF and the credential decoder.

## 1.2.14 — 2026-11-11

### Fixed

- `bialet -r` now runs `Db.init` before the script, so `Config.get()`,
  `Config.set()` and `Session.new()` work instead of failing with
  "no such table". There is still no request and the app's migration is still
  not run.
- `Query.toNum` returns `0` when the result is NULL or not numeric. `MAX()` /
  `SUM()` over an empty table no longer produce a `null` that crashes on the
  next `+ 1`.
- `Date.==` and `Date.!=` are null-safe: `date == null` is `false` instead of
  raising `Null does not implement 'unix'`. Comparing a Date to a non-Date is
  likewise `false` / `true`.
- `value == null` and `value != null` no longer dispatch to a user class's
  `==` with a null operand; null equality is decided by the VM, so a class
  that reads a field from the other operand cannot crash on null.
- `Num.fromString(null)` returns `null`, the same as an unparseable string,
  instead of raising "Argument must be a string".

### Added

- `value.toString()` is accepted as an alias for the `toString` getter on
  every type, including user classes. `count()`, `keys()` and `values()` are
  still getters, and `trim` still needs its parentheses.
- `Map.get(key)` / `Map.get(key, default)` and a non-mutating `List.reverse`.
- `Date.fromUnix(seconds)` builds a Date from epoch seconds in `Date.tz`.
- `Test.textContains()` and `Test.text()` assert on visible text (tags
  stripped, whitespace collapsed) instead of raw markup.
- A bare `%` in a Wren string is a literal; only `%(` starts interpolation.

### Performance

- Identifier tokens are lexed from the source slice instead of through a
  temporary buffer. Per-request VM creation compiles the core sources, so this
  is the largest win: about 20% lower latency on the interpreter endpoints in
  the benchmark harness.
- `String.safe`, `String.trim`/`trimStart`/`trimEnd` (no arguments) and
  `String.split` are native, and `Util.secureEquals` compares bytes directly.
  Template escaping is about 35% faster, rendering about 17%, and query-heavy
  requests about 19%.
- `Query.fetch` builds column names once per result set instead of once per
  row.

## 1.2.13 — 2026-11-11

### Fixed

- A SQL statement that fails now raises a catchable Wren error instead of
  being logged and ignored. In particular, `Db.migrate` no longer records a
  version whose SQL never ran, so a broken migration can be fixed and retried
  on the next boot.
- The test client keeps a cookie jar, so a session (and its CSRF token) minted
  by one `Test.get` is the session the following `Test.post` presents. Tests
  no longer need a `test_mode` flag to exercise a CSRF-protected form.
  `Session.csrf` / `Session.csrfOk` are also available as statics, and
  `Test.csrf()` reads the token a form rendered.
- `Request.route()` no longer includes the query string. `/notes?page=2` used
  to resolve as the detail route `?page=2`, so a `.wren` route file that reads
  `Request.route(0)` answered 404 as soon as a query parameter was present.
  Pagination and filters now work on path-based routes.
- Every response carries `Connection: close`. The status line was already
  HTTP/1.1, but without the header a client was entitled to assume a
  persistent connection the server never provided. The docs now say HTTP/1.1
  with one request per connection instead of HTTP/1.0.

### Changed

- The documentation explains why Bialet embeds Wren, what that costs, and
  includes a syntax mapping for JavaScript and Python developers.
- The comparisons now cover Go and PocketBase, add the HTMX pattern, and
  correct the PHP, Express, and Flask tables: SQLite ships with PHP and
  Python, and neither framework requires an ORM or a template engine.
- A measured [performance page](docs/performance.md) publishes latency,
  throughput, startup time, and RSS for macOS arm64 and Linux arm64.
- [Deployment](docs/deployment.md) gains backup, restore, upgrade, and rollback
  sections.
- The examples page loses the redundant counter example and the decorative
  emoji and inline styles, and gains a complete authenticated inventory app
  with sessions, search, pagination, validation, and an upload.
- The login documentation used `session.login()`, which does not exist; it now
  shows `Session.rotate()` plus `session.set("user_id", ...)`.
- A [showcase page](docs/showcase.md) publishes 22 days of production traffic
  for home.dev.ar, including the scanner-heavy request mix.
- Releases publish `SHA256SUMS`, and the install script verifies the archive it
  downloads.

## 1.2.5

### Fixed

- `Request.post()` now reads the text fields of a `multipart/form-data` form.
  Before, it returned `null` for every field of a form that also uploaded a
  file, which also broke `Session.csrfOk` on those forms. The fields are
  extracted in C while the file parts are saved, so file content is never copied
  into Wren. Fields are bounded by `--max-post`.
- A multipart part with no blank line after its headers no longer swallows the
  parts that follow it. The search for the end of a part's headers is bounded by
  that part, which also removes a quadratic rescan of the body for requests made
  of many such parts.
- A file part without a `name` is no longer stored under the value of another
  header parameter.
- A `boundary` longer than 255 bytes is rejected instead of being truncated.

### Changed

- `Request.body` is an empty string for `multipart/form-data` requests. It used
  to hold the whole payload with its line breaks removed and the upload
  truncated at the first zero byte. Read the form with `Request.post()` and
  `Request.file()`. Other content types are unchanged.
- `Request.init` takes a fourth argument, `fields`. Bialet calls it, so apps are
  not affected.
- A quoted `boundary="..."` in the `Content-Type` header and escaped quotes in a
  part's `name` are now understood. Reaching the file limit (100 per request)
  no longer drops the text fields that come after it.
