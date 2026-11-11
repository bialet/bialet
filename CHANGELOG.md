# Changelog

Releases 1.2.6–1.2.12 were tagged and published on the
[releases page](https://github.com/bialet/bialet/releases); this file resumes at
1.2.13.

## 1.2.13 — 2026-11-10

### Fixed

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
