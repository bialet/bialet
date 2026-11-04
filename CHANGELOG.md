# Changelog

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
