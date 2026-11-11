# Roadmap

Bialet is stable for personal use. Items under *Priority* are real
dependencies of deployments that run on it and take priority. The rest are
ideas I may revisit if interest returns, but no promises.

## Priority

- [x] **Expose upload and SQLite limits as startup config** — `-u /
      --max-upload-size` (bytes or K/M/G suffix; default 4 MB), `-f /
      --foreign-keys` (on/off), and `-s / --synchronous` (off/normal/full/
      extra). The upload cap is clamped at startup to what the request-body
      cap (`-b` / `-m`) can actually deliver, so rejection logs state a
      reachable limit; docs updated to match (default effective upload is ~120
      KB until `-b` and `-m` are raised).
- [ ] **`bialet check` — whole-tree validation** — `-t` validates a single
      `.wren` file. A deploy hook needs a command that validates every `.wren`
      in an app tree and exits `0`/`1`. The docs and manifesto advertise
      `init`/`check`/`test`, but only `validate`/`tests`/`dev` exist.
- [ ] **Hidden health endpoint** — a reserved `_`-prefixed route (e.g.
      `/_health`) returning version, uptime, and DB status so supervisors and
      watchdogs can health-check an app without relying on an app route.

## Editors/IDE Support

- [x] VS Code (syntax JSON, language-configuration)
- [ ] Sublime Text (.sublime-syntax and .tmPreferences)
- [x] Vim / Neovim (ftdetect, syntax)
- [ ] JetBrains (IntelliJ) (TextMate bundle or custom plugin)

## On my radar

- [ ] **Per-request access logs** — `-l` should record one line per request
      (method, path, status, duration, bytes) so a host can meter usage and
      detect abuse without parsing proxy logs.
- [ ] **Optional on-disk file storage** — a flag to store uploads on disk
      (under the app's data dir, outside the code release) instead of only as
      DB blobs. Large files currently inflate the database, backups, and the
      disk quota all at once.
- [ ] **`bialet init`** — scaffold a new app directory, completing the
      `init`/`check`/`test` story the docs promise.
- [ ] **Admin dashboard** — External lib UI for browsing the database and
      viewing logs
- [ ] **Data filtering lib** — External lib for query sanitization and type-safe
      filtering utilities
- [ ] **i18n / localization** — Translation helpers and locale-aware formatting
- [ ] **LSP support** — Language Server Protocol implementation with
      autocomplete, go-to-definition, and diagnostics for VS Code
- [ ] **Opcode Cache** — Compile scripts to cached bytecode for faster request
      handling
- [ ] **HTTP Client - Multipart / file uploads** — no way to send files or
      `multipart/form-data` to an external API.
- [x] **HTTP Client - Response cookies / cookie jar: per-host scoping** — the
      jar is process-wide and sends cookies regardless of domain; scope by host
      and honor `Domain`/`Path`/`Secure` attributes.
- [x] **Query params silently drop non-primitive values** — `queryPrepare`
      only binds `null`/`bool`/`num`/`string` params; any other value (e.g. an
      `HtmlNode`) is skipped, shifting every later column left and silently
      corrupting the row. Stringify non-primitive params (via `toString`) or
      error loudly instead of dropping them.

## Security follow-ups

Confirmed findings from the run-5/run-6 audits shipped in 1.2.9 and 1.2.10. The
six run-7 findings ship in 1.2.15: a case-variant `.wren` URL returned route
source instead of executing it, `Request.login` base64-decoded the credential
quadratically (one 4 KB header ≈ 0.5 s of the single worker), the session-table
sweep was gated on per-request VM state and never ran, an `-l` log file inside
the app root was downloadable, the Markdown inline pass was quadratic in
unmatched `[`, and password hashing truncated the credential at the first NUL.
These are the remaining leads and hardening gaps from those runs and from
run-7.

- [ ] **Win32 TLS handshake deadline** — `SSL_connect` is bounded per socket
      read, not as a whole: a peer that drips one byte per interval can hold the
      single worker far past the hop budget. Needs a non-blocking handshake loop
      (or an out-of-band cancellation channel).
- [ ] **Uncancellable DNS on Windows** — `getaddrinfo` has no timeout and cannot
      be interrupted; the hop budget now detects an overrun but cannot abort the
      resolver. Move resolution off the request thread or bound it externally.
- [ ] **Win32 URL parser parity** — `parse_url` ends the authority only at `/`
      or `:`, so a query-only URL, IPv6 literal, uppercase scheme, trailing dot
      or non-default port fails differently from `Http.urlScope_`/libcurl
      (`Host:` omits the port). Align the splitter and strip the fragment from
      the initial URL. The port omission is confirmed: `build_request` formats
      `Host: %s` from `url->host` only, so a call to a vhost-routed listener on
      a non-default port presents the wrong authority on Windows while the
      POSIX path lets libcurl write the full one.
- [ ] **Case-insensitive framework headers** — a caller spelling
      `content-type`/`authorization` non-canonically gets duplicate header
      lines, and the Win32 transport can emit two `Authorization` lines; use one
      case-insensitive lookup and replace instead of append.
- [ ] **Configurable credential strip list** — cross-origin redirects drop only
      `Authorization` and `Cookie`; a caller-supplied `Proxy-Authorization` (or
      custom auth header) still follows the hop. Offer an opt-in strip list.
- [ ] **Document `Session.rotate()`** — the login example never calls it, and
      the session id is client-adoptable, so an app that follows the docs stays
      fixation-vulnerable. Call it in the example and state that it must precede
      any authenticated write. The adoption is confirmed: any shape-valid
      `BIALETSESSID` is kept verbatim with no re-mint, and the CSRF token is
      stored under the same id, so an app that skips `rotate()` hands the
      planter a valid token too.
- [ ] **Session read-side staleness** — after `rotate()`/`destroy()` an existing
      `Session` still serves its constructor snapshot from `get()`/`id` until
      the next `set()`. Reconcile with the live cookie or document the window.
- [ ] **Length-bounded header map** — the non-multipart request is handed to
      Wren with `wrenSetSlotString`, which truncates at an embedded NUL; use
      `wrenSetSlotBytes` with the framed length on both paths.
- [ ] **Index the upload pool for eviction** — eviction sorts up to 4096 rows
      per deleted row (~1.7 ms each); an `(isTemp, createdAt, id)` index makes it
      O(log n) and removes a per-request cost multiplier.
- [ ] **Temporary upload ids are not durable** — a temp id is readable only
      inside the request that uploaded it; document that persistence needs
      `save()`.
- [ ] **Session eviction has no owner scope** — the `maxSessions` cap evicts
      the ids with the oldest `MAX(updatedAt)`, and the table carries no owner
      or authenticated marker, so an anonymous client that writes a fresh id per
      request evicts real users' sessions once the cap is reached and can force
      a global logout. Refuse new pre-auth rows at the cap, or record which ids
      were ever authenticated and evict only never-authenticated ones.
- [ ] **Escape C1 controls in the log** — `log_escape` covers C0 and DEL only,
      so 0x80–0x9F (CSI, OSC, ST, NEL) and their UTF-8 forms reach the operator
      log raw while the function's own comment promises every control byte is
      made inert. Confirm what the log consumer does with them, and escape the
      range either way.
- [ ] **Report an unterminated string literal** — `readString` has no newline
      case, so a quoted literal left open silently swallows the rest of the file
      as string data instead of failing with "Unterminated string". This is also
      what makes the `.inc` generator's line filtering only conditionally safe.
- [ ] **Compare a committed `.inc` with a regeneration** — a clean build always
      regenerates the embedded module text, so releases are safe, but `make`
      after a successful build reports a hand-edited `.inc` as up to date and
      compiles it. Add a `cmp` of a fresh regeneration in the pre-commit hook or
      CI.
- [ ] **Make the password KDF a hard build requirement** — `OPENSSL_OK` demands
      3.0 while `HAVE_SSL` accepts any OpenSSL, and every API the PBKDF2 path
      needs exists in 1.1.x, so a 1.1 build silently stores a 32-bit salted
      DJB2 digest and cannot verify a `pbkdf2$` row at all. Widen the gate to
      1.1 or fail the build with a clear message.
- [ ] **Sign the release, or anchor the checksum outside it** — `install.sh`
      verifies the archive against `SHA256SUMS` fetched from the same release,
      and a missing checksum is only a warning, so the check catches corruption
      but not an actor who can publish. Add a signature or attestation (or a
      digest in an independent channel) and make its absence fatal.
- [ ] **Guard the release trigger** — `release.yml` runs on any pushed `v*` tag
      with no protected environment, repoints `docker:latest` in parallel with
      the build jobs, and only asserts `BIALET_VERSION` in the PPA job after the
      release and the image are already published. Require the tag to be on
      `main`, check the version before publishing, and gate the job behind an
      environment.
- [ ] **Never interpolate a ref name into `run:`** — `release.yml` and `ppa.yml`
      build shell text from `${{ github.ref_name }}` / `${{ inputs.tag }}`, so a
      tag containing a quote and `$(…)` is shell code in the steps holding the
      Homebrew deploy key and the PPA signing key. Use the `env:` form the
      Docker job already uses.

## Someday maybe

- [ ] **Official "hosted" deployment recipe** — a doc page showing the
      recommended hosted pattern: immutable release root + external DB via
      `-d` + `umask 0077`. `deployment.md` covers the pieces; this would be
      the full recipe.
- [ ] **Warn when a block callback returns null** — a multi-statement `map`
      callback (or any block whose body is not a single expression) renders
      empty output with no error. Log a warning so a silent empty `<ul>` is
      distinguishable from an empty query result.
- [ ] **WebAssembly target** — Compile Bialet apps to Wasm for edge deployment
- [ ] **MySQL and PostgreSQL support** — Optional alternative database backend
- [ ] **HTTPS / TLS support** — Native TLS in the server binary (no reverse
      proxy needed for basic deployments)

## How to Contribute

See [CONTRIBUTING.md](CONTRIBUTING.md) for setup and guidelines.

Feature requests and feedback are welcome. Open an issue with the "enhancement"
label on [GitHub Issues](https://github.com/bialet/bialet/issues).
