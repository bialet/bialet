# Forms

Bialet handles form submissions with plain HTML forms and a few Wren methods.
No magic, no auto-binding — a `<form>` sends data, your `.wren` file reads it,
and you decide what happens next.

## Quick Start

```wren
// contact.wren — handles /contact

if (Request.isPost) {
  var name = Request.post("name") || ""
  var email = Request.post("email") || ""
  var message = Request.post("message") || ""

  // Validate
  if (name == "" || email == "" || message == "") {
    return <p class="error">All fields are required.</p>
  }

  // Save to database
  `INSERT INTO messages (name, email, message) VALUES (?, ?, ?)`.query([name, email, message])

  // Redirect to avoid resubmission
  return Response.redirect("/contact?sent=1")
}

// GET request — show the form
return <main>
  <h1>Contact Us</h1>
  {{ Request.get("sent") && <p class="success">Message sent!</p> }}
  <form method="post">
    <label>Name: <input name="name" /></label>
    <label>Email: <input name="email" type="email" /></label>
    <label>Message: <textarea name="message"></textarea></label>
    <button type="submit">Send</button>
  </form>
</main>
```

## The Form Pattern

Every form-handling page follows the same three-step pattern:

1. **Check** — `Request.isPost` determines if the form was submitted
2. **Process** — read fields, validate, save to database
3. **Redirect** — send the browser elsewhere to prevent duplicate submissions

> This is the Post/Redirect/Get (PRG) pattern. Without the redirect, refreshing
> the page resubmits the form.

```wren
if (Request.isPost) {
  var title = Request.post("title") || ""
  `posts`.save({"title": title})
  return Response.redirect("/posts")
}

return <form method="post">
  <input name="title" />
  <button>Save</button>
</form>
```

## Reading Form Fields

`Request.post(name)` returns the value of a form field, or `null` when the
field is missing.

> ⚠️ Pitfall: `Request.post(name)` returns `null` when the key is missing.
> Always provide a fallback with `|| ""` or a null check before calling any
> string method on the result.

```wren
var name = Request.post("name") || ""

// For fields where empty string is also invalid
var email = Request.post("email")
if (!email) {
  return <p>Email is required</p>
}
```

## Validation

Bialet has no built-in validation framework. You check values with plain
`if` statements and return error HTML when validation fails.

```wren
if (Request.isPost) {
  var name = Request.post("name") || ""
  var email = Request.post("email") || ""

  var errors = []

  if (name.trim == "") errors.add("Name is required")
  if (email.trim == "") errors.add("Email is required")

  if (errors.count > 0) {
    return <main>
      <h1>Validation Errors</h1>
      <ul>{{ errors.map {|e| <li>{{ e }}</li> } }}</ul>
      {{ formHtml }}
    </main>
  }

  `users`.save({"name": name, "email": email})
  return Response.redirect("/users")
}
```

### Common Validations

```wren
// Required field
if (Request.post("title") || "" == "") {
  return <p>Title is required</p>
}

// Numeric range
var age = Num.fromString(Request.post("age") || "0")
if (age < 18 || age > 120) {
  return <p>Age must be between 18 and 120</p>
}

// Checkbox (returns null when unchecked)
var agreed = Request.post("terms") == "on"

// Radio button or select
var choice = Request.post("vote") || ""
if (choice == "") {
  return <p>Please select an option</p>
}
```

## Redirect After POST

Always redirect after a successful form submission. Use
`return Response.redirect(path)` — both keywords are required.

```wren
// Correct — both return and redirect
if (Request.isPost) {
  `tasks`.save({"description": Request.post("description") || ""})
  return Response.redirect("/")
}

// Wrong — missing return, causes a double-response error
if (Request.isPost) {
  `tasks`.save({"description": Request.post("description") || ""})
  Response.redirect("/")
}
// Execution continues here and tries to send a second response
```

Pass success flags through query parameters:

```wren
return Response.redirect("/?created=1")
```

Then read it on the next page:

```wren
{{ Request.get("created") && <p class="success">Item created!</p> }}
```

## CSRF Protection

Bialet includes built-in CSRF tokens through `Session`. Add the token to
every state-changing form and verify it on submission.

```wren
var session = Session.new()

if (Request.isPost) {
  if (!session.csrfOk) {
    Response.status(403)
    return <p>Invalid form submission.</p>
  }
  // Process the form...
  return Response.redirect("/")
}

return <form method="post">
  {{ session.csrf }}
  <input name="title" />
  <button>Save</button>
</form>
```

- `Session.csrf` renders a hidden `<input>` field with the token
- `Session.csrfOk` returns `true` when the submitted token matches

See the [Security](security.md) guide for details on CSRF and the
`Secure`/`HttpOnly`/`SameSite` cookie defaults.

## File Uploads

Set `enctype="multipart/form-data"` on your form to upload files. Bialet
stores files in the SQLite database, not on disk.

```wren
if (Request.isPost) {
  var file = Request.file("attachment")

  if (!file) {
    return <p>No file was uploaded.</p>
  }

  return <p>Uploaded: {{ file.name }} ({{ file.size }} bytes)</p>
}

return <form method="post" enctype="multipart/form-data">
  <input type="file" name="attachment" />
  <button>Upload</button>
</form>
```

Files are permanent by default when accessed through `Request.file()`. You
can mark a file as temporary with `file.temporary()` — temporary files are
automatically deleted within a day.

```wren
var file = Request.file("attachment")
if (file) {
  file.temporary()  // Cleaned up automatically
  var content = file.read()
  // Process the content...
}
```

The maximum upload size is 10 MB by default.

To serve a file back, use `Response.file(id)`:

```wren
var file = `SELECT id, name FROM BIALET_FILES WHERE id = ?`.first([id])
if (file) {
  return Response.file(id)
}
```

See the [File Handling](file.md) guide for the full file API.

## Forms Without JavaScript

Bialet forms work without JavaScript. Use standard HTML attributes for
client-side behavior:

```wren
<form method="post">
  <input name="title" required />
  <input name="email" type="email" required />
  <textarea name="body" minlength="10"></textarea>
  <input name="age" type="number" min="18" max="120" />
  <button>Submit</button>
</form>
```

The `required`, `type`, `min`, `max`, and `minlength` attributes trigger
browser-native validation first. Server-side validation is still necessary
— never rely on client-side checks alone.

## Forms with `_route.wren`

When using a dynamic route, the form still lives in the same file:

```wren
// admin/posts/_route.wren

if (Request.isPost) {
  var postId = Request.post("id") || ""
  var title = Request.post("title") || ""
  `posts`.save({"id": postId, "title": title})
  return Response.redirect("/admin/posts")
}

var id = Request.route(0)
var post = `SELECT * FROM posts WHERE id = ?`.first([id])

return <main>
  <h1>Edit Post</h1>
  <form method="post">
    <input type="hidden" name="id" value="{{ post["id"] }}" />
    <label>Title: <input name="title" value="{{ post["title"].safe }}" /></label>
    <button>Save</button>
  </form>
</main>
```

> ⚠️ Pitfall: when pre-filling form values from the database, escape them
> with `.safe` to prevent attribute-injection attacks. Any string you output
> into an HTML attribute must be escaped.

## Pitfalls

- **`Request.post()` returns `null`** — always fall back with `|| ""` or a
  null check before string operations.
- **Missing `return` before `Response.redirect()`** — the redirect sends
  headers, then the code below still runs and tries to send a body.
- **No `.safe` on pre-filled values** — database values interpolated into
  HTML must be escaped, including in `<input value="...">` attributes.
- **Forgetting CSRF on state-changing forms** — browsers with SameSite=Lax
  may block some CSRF, but the explicit token check is still required.
- **Relying on client-side validation alone** — browser-side `required` and
  `type="email"` are conveniences, not security. Always validate on the
  server.

## Next Steps

- Set up CSRF protection with [Session](security.md)
- Learn about handling [files](file.md) in detail
- Understand [routing](routing.md) for where to place your form handlers
- See [database](database.md) for saving form data with parameterized queries
