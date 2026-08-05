# Security

Bialet keeps the framework small, and that extends to security: there is no
magic that escapes your output for you, and no ORM that hides your SQL. The
good news is that the sharp edges are explicit and few. This page is the
central reference for keeping a Bialet app safe.

Three rules cover most real-world damage:

1. **Escape untrusted output** — `{{ }}` does not escape. Use `.safe`.
2. **Parameterize SQL** — never build a query from string concatenation.
3. **Validate state-changing requests** — check `Session.csrfOk` on POSTs.

## Built-in Protections

| Protection | Where | Notes |
|------------|-------|-------|
| Parameterized SQL | Backtick Query objects | String interpolation is rejected by the compiler |
| HTML escaping | `.safe`, `Util.htmlEscape()` | You must call it — never automatic |
| CSRF tokens | `Session.csrf` / `Session.csrfOk` | Constant-time comparison |
| HttpOnly + SameSite cookies | `Cookie.set` defaults | `Secure` added automatically behind TLS |
| Private file blocking | Server | `_` and `.` prefixed files return 403 |
| Salted password hashes | `Util.hash` / `Util.verify` | Never store plaintext |
| Constant-time compare | `Util.secureEquals` | Used by `csrfOk`, available to you |

## Cross-Site Scripting (XSS)

The `{{ }}` interpolation **does not escape HTML by default**. When you
display user-generated content, database values, or URL parameters, you must
escape them yourself. This is deliberate: it keeps inline HTML readable and
fast, and it makes the escaping explicit where it matters.

```wren
var userInput = "<script>alert('xss')</script>"

// WRONG — XSS vulnerability
var dangerous = <p>{{ userInput }}</p>
// Renders: <p><script>alert('xss')</script></p>

// CORRECT — HTML characters are escaped
var safe = <p>{{ userInput.safe }}</p>
// Renders: <p>&lt;script&gt;alert(&#x27;xss&#x27;)&lt;/script&gt;</p>
```

`.safe` replaces `&`, `<`, `>`, `"`, and `'` with their HTML entities. The
equivalent helper `Util.htmlEscape(str)` returns the same result as a plain
string:

```wren
var escaped = Util.htmlEscape(userInput)
```

> ⚠️ Pitfall: **Forgetting `.safe` is an XSS vulnerability.** Every string
> that comes from user input, the database, or the URL must be escaped before
> it reaches HTML. There is no automatic escaping — not even for values pulled
> from `Request.post()`.

Escaping applies in attributes too, not just element bodies:

```wren
// WRONG — break out of the attribute, inject an event handler
<a href="{{ url }}">link</a>

// CORRECT
<a href="{{ url.safe }}">link</a>
```

If you generate HTML in Wren code rather than inline blocks, escape with
`Util.htmlEscape()` before concatenating it into a template string.

> ⚠️ Pitfall: `.safe` escapes for HTML text and attribute contexts. It does
> not sanitize JavaScript, CSS, or URLs. Never interpolate untrusted input
> into a `<script>` block or a `javascript:` URL — restructure the page so it
> cannot happen.

## SQL Injection

Backtick Query objects use **prepared statements** with `?` placeholders.
The query string and the values are sent to SQLite separately, so values
cannot change the structure of the SQL.

```wren
// CORRECT — parameterized
`SELECT * FROM users WHERE name = ? AND active = 1`.fetch(name)

// CORRECT — multiple parameters
`INSERT INTO messages (text, session) VALUES (?, ?)`.query(msg, Session.id)

// WRONG — never build SQL from strings
`SELECT * FROM users WHERE name = '%(name)'`.fetch
```

You cannot concatenate or interpolate into a backtick Query object — the
compiler rejects it. That constraint removes the most common injection path
entirely. Stay inside it.

Two places tempt people out of it:

- **Dynamic `ORDER BY` columns.** Column names cannot be placeholders. Use
  the `order()` method with an allow-list of column names:

  ```wren
  `SELECT * FROM products`.order("price", "ASC",
      ["price", "name", "createdAt"], 50).fetch
  ```

- **`LIMIT` / `OFFSET`.** These accept only integers. Parameterize them —
  never concatenate user input:

  ```wren
  `SELECT * FROM products LIMIT ? OFFSET ?`.fetch([limit, offset])
  ```

> ⚠️ Pitfall: database values come back as strings. Before using one as a
> number, convert with `Num.fromString(...)` or `query.num`. This is a
> correctness concern, but it also stops "it works when I type a number"
> bugs from becoming injection-adjacent string handling.

See the [Database](database.md) and [Advanced Routing](advanced-routing.md) guides for more
Query examples.

## CSRF Protection

Bialet ships built-in CSRF tokens via `Session`. A token is generated per
session, stored server-side, and rendered as a hidden form field. On
submission, the token in the request is compared against the stored token
using `Util.secureEquals` — a constant-time comparison that does not leak
timing information.

```wren
var session = Session.new()
var verified

if (Request.isPost) {
  verified = session.csrfOk
  if (verified) {
    `INSERT INTO messages (text) VALUES (?)`.query(Request.post("msg") || "")
  }
}

return <form method="POST">
  {{ session.csrf }}
  <input name="msg">
  <button>Submit</button>
</form>
```

- `Session.csrf` returns a `<input type="hidden" name="_bialet_csrf" ...>`
  field. Put it inside every form that changes state.
- `Session.csrfOk` returns `true` when the submitted token matches the stored
  one. Check it on every POST (and PUT/DELETE via `Request.method`).

> ⚠️ Pitfall: the session cookie is `SameSite=Lax`, which already stops most
> cross-site POSTs. Do not treat that as enough. SameSite is advisory
> (older browsers ignore it) and it does nothing for same-site subdomain
> attacks. Keep the explicit token check on any form that writes data.

## Sessions & Cookies

Cookies set through `Cookie.set` get secure defaults:

| Attribute   | Default    | Notes                                      |
|-------------|----------- |--------------------------------------------|
| `Path`      | `/`        |                                            |
| `HttpOnly`  | `true`     | Not readable from JavaScript               |
| `SameSite`  | `Lax`      | Blocks most cross-site cookie sending      |
| `Secure`    | auto       | Added when the request arrives over HTTPS  |

You can override any of these by passing an options map, for example
`Cookie.set("pref", "dark", {"SameSite": "Strict"})`. Be careful overriding
`HttpOnly` or `Secure` — both are there for a reason.

Sessions are stored in SQLite in the `BIALET_SESSION` table. The session
cookie (`BIALETSESSID` by default) holds only a 40-character random ID; all
data lives server-side. Clear a session with `Session.destroy()`.

The table is keyed on `(id, key)`: `Session.set` replaces the row for a key
instead of appending, and `Session.get` always returns the latest value
written. Older databases that lack the primary key are rebuilt in place on
startup.

> ⚠️ Pitfall: `Request.post(name)` returns `null` when the key is missing.
> Calling string methods on it crashes the request. Always guard with `|| ""`
> or a null check:

> ```wren
> var msg = Request.post("msg") || ""
> ```

Cookie names and values are validated (`Util.cookieToken`,
`Util.cookieValue`) and reject characters that could break out of the
`Set-Cookie` header, so a user-controlled value cannot inject headers.

## Password Storage

Use `Util.hash` and `Util.verify` instead of rolling your own:

```wren
var stored = Util.hash(password)          // store this in the database
var ok     = Util.verify(password, stored) // true if the password matches
```

`hash` produces a salted SHA-256 digest in `hash$salt` format. The salt is
random per password, so identical passwords produce different stored values,
and the stored format is all you need to keep — `verify` parses it back out.

Be honest about the limits: this is salted SHA-256, a fast hash. It is fine
for internal tools and low-stakes apps. For user-facing accounts that face
real attackers, consider a deliberately slow KDF (bcrypt, scrypt, argon2)
implemented at the app or proxy layer.

## Secure Configuration & Deployment

### TLS

Bialet speaks **HTTP/1.0** and has no native HTTPS. Run it behind a reverse
proxy (nginx, Apache, or Caddy) for TLS. This is the recommended production
setup, and it also lets you bind Bialet to `127.0.0.1` so only the proxy
talks to it. See [Deployment](deployment.md) for configs.

When requests arrive through a proxy, Bialet detects HTTPS from the
`X-Forwarded-Proto` / `Forwarded` headers. That is how the `Secure` cookie
attribute is decided. Only set those headers from a proxy you trust — never
forward user-supplied ones unchanged.

### Reverse Proxy Hardening

Bialet is **single-threaded**: it accepts and serves one connection at a time
in a blocking loop. The reverse proxy is the layer that protects it from
hostile clients. Configure the proxy to:

- **Cap the request body.** Bialet accepts bodies up to ~10 MB. Parsing large
  bodies is expensive: every byte of a decoded value allocates, so a ~10 MB
  body still means millions of allocations in `Util.urlDecode`. Set
  `client_max_body_size`
  (nginx), `LimitRequestBody` (Apache), or an equivalent to the smallest your
  app needs — 1 MB is a sane default.
- **Enforce a total body-read deadline.** Bialet's 5-second socket timeout is
  per `recv()` call, so a peer that dribbles bytes slowly can hold a
  connection open indefinitely. Set a total read timeout at the proxy
  (`client_body_timeout` in nginx, `RequestReadTimeout ... MinRate` in
  Apache) so stalled uploads are cut off.
- **Buffer the full request before forwarding** (`proxy_request_buffering on`
  in nginx; the default in most proxies). This keeps a slow client from
  holding the upstream socket.
- **Deny private files at the proxy too.** Bialet already returns 403 for
  `_`/`.`-prefixed paths, but the proxy can return 403 first — a second layer
  in case an app or a planted symlink ever serves such a file.

See [Deployment](deployment.md) for ready-to-paste nginx, Apache, and Caddy
configs with these settings.

> ⚠️ Pitfall: a proxy body cap that is *higher* than your app's needs
> re-opens the CPU-exhaustion and memory-exhaustion risks above. Keep it as
> low as your uploads actually require.

### Private Files

Files and directories whose name starts with `_` or `.` are forbidden from
direct HTTP access — the server returns 403. This is what protects
`_app.wren`, `_migration.wren`, `_db.sqlite3`, and your configuration from
being downloaded. Name anything private with a leading `_` or `.`.

> ⚠️ Pitfall: `_db.sqlite3` contains your data and your session table.
> It is already blocked from HTTP access, but the app directory on disk is
> not a sandbox. Keep it out of version control backups you share, and
> restrict filesystem access to the user that runs the server.

### Database File Permissions

Bialet creates `_db.sqlite3` with SQLite's default mode, `0666 & ~umask`.
Under the common `umask 022` that yields a world-readable `0644` file, so
any local user on a shared host can read the whole database — uploaded
files, password hashes, and cached remote modules included. The server does
not tighten the mode after opening the file.

Run Bialet with a restrictive umask so the file is created `0600`:

```bash
umask 0077
bialet -p 7001 /www/myapp
```

Under systemd, set `UMask=` on the service instead:

```ini
[Service]
UMask=0077
```

If the database already exists, tighten it with `chmod`. Redo this after
any migration or restore that recreates the file:

```bash
chmod 600 /www/myapp/_db.sqlite3
```

> ⚠️ Pitfall: a world-readable `_db.sqlite3` is as exposed as a leaked
> backup — no HTTP request is needed to read it. Restrict the app root to
> the user that runs the server and keep the file out of shared backups.

### Resource Limits

The server can enforce memory and CPU ceilings per app with CLI flags, which
limits the blast radius of a runaway script or a memory-exhaustion attack:

```bash
bialet -p 7001 -m 1024 -M 2048 -c 25 -C 50 /www/myapp
```

- `-m` / `-M` — soft / hard memory limit in MB
- `-c` / `-C` — soft / hard CPU limit in percent

See the resource limits table in [Deployment](deployment.md) for defaults.

### Other Defaults

- SQLite `PRAGMA foreign_keys` is **on** by default, so orphaned rows
  cannot accumulate through a careless delete.
- File uploads are stored inside the SQLite database, not on disk — they
  are never served from a writable directory. See the `File` class in the
  [Reference](reference.md).
- Validate user input before trusting it: check types and ranges, convert
  with `Num.fromString()`, and reject values you did not expect. Do this in
  your domain classes before saving, and again in the form handler.

## Security Checklist

1. Escape every string from user input, the database, or the URL with `.safe`
   before interpolating into HTML.
2. Never build SQL from strings — use `?` placeholders and pass values as
   parameters.
3. Use `.order()` with an allow-list for sortable columns.
4. Put `{{ session.csrf }}` in every state-changing form and check
   `session.csrfOk` on POST.
5. Guard `Request.post(...)` with `|| ""` before string operations.
6. Store passwords with `Util.hash` / `Util.verify`, never plaintext.
7. Run behind a reverse proxy with TLS; bind Bialet to `127.0.0.1`.
8. Cap the request body and set a total read deadline at the proxy; buffer
   the full body before forwarding.
9. Keep private files (`_`-prefixed) out of shared backups and repositories.
10. Use `-m` / `-M` / `-c` / `-C` to cap memory and CPU.
11. Validate and convert input with `Num.fromString()` before numeric math.
