# Forms

Bialet handles forms with plain HTML and a few Wren methods. A `<form>`
sends data, your `.wren` file reads it, and you decide what happens next.

## The Pattern

Every form page follows the same three steps:

```wren
if (Request.isPost) {
  // 1. Read the fields
  var name = Request.post("name") || ""

  // 2. Process (validate, save)
  `users`.save({"name": name})

  // 3. Redirect to avoid resubmission
  return Response.redirect("/users")
}

// GET — show the form
return <form method="post">
  <input name="name" />
  <button>Save</button>
</form>
```

> ⚠️ Pitfall: `Request.post(name)` returns `null` when the key is missing.
> Always guard with `|| ""` or a null check before string operations.

## Subscribe Form (POST + Validation)

A simple email subscription form with basic validation:

```wren
// subscribe.wren

var email = Request.post("email") || ""
var error = null

if (Request.isPost) {
  if (!email.contains("@")) {
    error = "Enter a valid email."
  } else {
    `INSERT INTO subscribers (email) VALUES (?)`.query(email)
    return Response.redirect("/subscribe?ok=1")
  }
}

return <main>
  <h1>Subscribe</h1>
  {{ Request.get("ok") && <p class="success">Subscribed!</p> }}
  {{ error && <p class="error">{{ error }}</p> }}
  <form method="post">
    <label>Email: <input name="email" type="email" value="{{ email.safe }}" required /></label>
    <button>Subscribe</button>
  </form>
</main>
```

Key points:
- `Request.post("email") || ""` reads the field safely
- `!email.contains("@")` is a quick sanity check — real email validation is
  complex; this catches most typos and empty submissions in one line
- `var error = null` and `{{ error && ... }}` renders the message inline
  instead of short-circuiting with `return`
- `value="{{ email.safe }}"` preserves the input on failed submission
- `return Response.redirect(...)` prevents resubmission on refresh
- `Request.get("ok")` shows a success message after redirect
- `type="email"` and `required` give free browser-side validation

> Never trust client-side validation alone. The `required` attribute is a
> convenience for the user, not a security boundary.

## Search Form (GET)

For search or filtering, use `GET` and read query parameters with
`Request.get()`:

```wren
// search.wren

var q = Request.get("q")
var results = []

if (q) {
  results = `SELECT * FROM posts WHERE title LIKE ?`.fetch("%" + q + "%")
}

return <main>
  <h1>Search</h1>
  <form method="get" action="/search">
    <input name="q" value="{{ q.safe }}" placeholder="Search posts..." />
    <button>Search</button>
  </form>

  {{ q && <section>
    <p>{{ results.count }} results for "{{ q.safe }}"</p>
    <ul>
      {{ results.map {|r| <li><a href="/post?id={{ r["id"] }}">{{ r["title"].safe }}</a></li> } }}
    </ul>
  </section> }}
</main>
```

Key points:
- `method="get"` puts the query in the URL (`/search?q=term`)
- `Request.get("q")` reads it — no need for `Request.post()`
- No redirect needed — GET forms are safe to refresh
- Pre-fill the input with `value="{{ q.safe }}"` so the term stays visible
- Escape all user input with `.safe` when outputting to HTML

## Login Form (POST + CSRF + Password)

A login form with CSRF protection and password verification:

```wren
// login.wren

var session = Session.new()
var email = Request.post("email") || ""
var error = null

if (Request.isPost) {
  if (!session.csrfOk) {
    error = "Invalid form submission."
  } else {
    var password = Request.post("password") || ""
    var user = `SELECT * FROM users WHERE email = ?`.first(email)

    if (user && Util.verify(password, user["password"])) {
      session.login(user["id"])
      return Response.redirect("/dashboard")
    }
    error = "Invalid email or password."
  }
}

return <main>
  <h1>Login</h1>
  {{ error && <p class="error">{{ error }}</p> }}
  <form method="post">
    {{ session.csrf }}
    <label>Email: <input name="email" type="email" value="{{ email.safe }}" required /></label>
    <label>Password: <input name="password" type="password" required /></label>
    <button>Login</button>
  </form>
</main>
```

Key points:
- `Session.csrf` renders a hidden token field — put it in every
  state-changing form
- `Session.csrfOk` verifies the token on submit — check it before processing
- `Util.verify(password, hash)` checks the password against the stored hash
  (see [Security](security.md) for how to hash passwords with `Util.hash`)
- `var error` with `{{ error && <p class="error">{{ error }}</p> }}` shows
  the message inline on a single form, no duplication
- `value="{{ email.safe }}"` preserves the email on failed login
- `session.login(id)` persists the session on success
- `return Response.redirect(...)` on success, no redirect on failure

> ⚠️ Pitfall: never store plaintext passwords. Use `Util.hash(password)` to
> create the hash, store that in the database, and verify with
> `Util.verify(password, storedHash)`. See the full [Security](security.md)
> guide for details on passwords, CSRF, and session defaults.

## File Uploads

Set `enctype="multipart/form-data"` on the form and read the file with
`Request.file()`:

```wren
if (Request.isPost) {
  var file = Request.file("attachment")
  if (file) {
    return <p>Uploaded: {{ file.name }} ({{ file.size }} bytes)</p>
  }
}

return <form method="post" enctype="multipart/form-data">
  <input type="file" name="attachment" />
  <button>Upload</button>
</form>
```

Files are stored in the SQLite database. See the [File Handling](file.md)
guide for the full API.

## Pitfalls

- **`Request.post()` returns `null`** — always `|| ""` before string ops.
- **Missing `return` before `Response.redirect()`** — causes double-response
  errors. Always `return Response.redirect(...)`.
- **No `.safe` on user input in HTML** — every untrusted string must be
  escaped, including in `<input value="...">` attributes.
- **Forgetting CSRF on state-changing forms** — `{{ session.csrf }}` in the
  form, `session.csrfOk` in the handler.
- **Plaintext passwords** — use `Util.hash` / `Util.verify`, never store raw
  passwords in the database.

## Next Steps

- [Security](security.md) — CSRF, password hashing, session defaults, XSS
- [File Handling](file.md) — uploading, serving, and managing files
- [Database](database.md) — saving form data with parameterized queries
- [Routing](routing.md) — where to place form handlers with query params vs
  `_route.wren`
