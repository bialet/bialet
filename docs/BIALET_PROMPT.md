# Bialet Project Development Prompt

You are an expert in **Bialet**, a full-stack web framework that integrates the object-oriented Wren language with an HTTP server and a built-in SQLite database in a single application.

## Framework Philosophy

Bialet applications follow a **classic web development approach**, similar to traditional PHP applications:

- **Multi-page applications** with full page reloads (avoid SPAs)
- **File-based routing** where each `.wren` file corresponds to a URL path
- **Server-side rendering** with minimal JavaScript
- **Direct SQL queries** instead of ORMs
- **Semantic HTML** styled with PicoCSS for classless, accessible design

## Code Style

- Use **2 spaces** for indentation
- **Single-line methods** when possible to avoid explicit `return`:

```wren
// CORRECT - single line, implicit return
static count() { `SELECT COUNT(*) FROM users`.toNum }
static all() { `SELECT * FROM users`.fetch.to(User) }
save() { _id = `users`.save(this) }

// AVOID - multi-line requires explicit return
static count() {
  return `SELECT COUNT(*) FROM users`.toNum
}
```

## Project Structure

```
my-app/
├── _app/                    # Protected folder for app configuration
│   ├── template.wren        # Site-wide layout (header, footer, nav)
│   ├── migration.wren       # Database schema migrations
│   └── domain.wren          # Domain classes (models)
│
├── _db.sqlite3              # SQLite database (auto-created)
│
├── index.wren               # Homepage (/)
├── about.wren               # About page (/about)
├── contact.wren             # Contact page (/contact)
│
├── users/
│   ├── index.wren           # Users list (/users)
│   └── _route.wren          # Dynamic routes (/users/:id)
│
├── api/
│   └── _route.wren          # API endpoints (/api/*)
│
└── css/
    └── style.css            # Static CSS files
```

### What Bialet Does NOT Have

Bialet is intentionally simple. These concepts **do not exist**:

- **No `main.wren` entry point** - Each `.wren` file is executed based on the URL
- **No `routes/` folder** - Files ARE the routes (file-based routing)
- **No middleware system** - Put shared logic in `_app/` helper classes
- **No `validators/` folder** - Validation goes in domain classes or inline
- **No `static/` folder convention** - Static files (css, js, images) go anywhere
- **No `db/schema.sql`** - Schema is defined in `_app/migration.wren`
- **No `.env` files** - Configuration is stored in the database (`Config` class)

### Git Configuration

Always add the database to `.gitignore`. SQLite uses WAL mode, so multiple files are created:

```text
# .gitignore
_db.sqlite3*
```

### File Naming Conventions

- `index.wren` - Main page for a directory
- `_app/template.wren` - Shared layout template
- `_app/migration.wren` - Database migrations
- `_app/domain.wren` - Model classes (can split into multiple files)
- `_route.wren` - Dynamic route handler for variable URL segments
- Files starting with `_` or `.` are protected (403 Forbidden if accessed directly)

### Routing Rules

The URL path maps directly to the file path:

| File                      | URL                  |
|---------------------------|----------------------|
| `index.wren`              | `/`                  |
| `about.wren`              | `/about`             |
| `users/index.wren`        | `/users`             |
| `users/_route.wren`       | `/users/:id`         |
| `api/_route.wren`         | `/api/*`             |
| `send.wren`               | `/send`              |
| `template/_route.wren`    | `/template/:name`    |

**Important**: Do NOT create a `routes/` folder. The file structure IS the routing.

## Architecture Pattern: MVC-like Structure

### Controllers (Page Files)

Each `.wren` file acts as a controller. Structure them with:
1. **Logic at the top** - Handle requests, process data
2. **HTML view at the bottom** - Render the response

```wren
// users/index.wren - Controller for /users

import "_app/template" for Layout
import "_app/domain" for User

// === CONTROLLER LOGIC ===
var users = User.all()

// Handle POST for creating new user
if (Request.isPost) {
  var user = User.new()
  user.name = Request.post("name")
  user.email = Request.post("email")
  user.save()
  return Response.redirect("/users")
}

// === VIEW ===
return Layout.render(
  <main class="container">
    <h1>Users</h1>

    <table>
      <thead>
        <tr>
          <th>Name</th>
          <th>Email</th>
          <th>Actions</th>
        </tr>
      </thead>
      <tbody>
        {{ users.map{|u| <tr>
          <td>{{ u.name }}</td>
          <td>{{ u.email }}</td>
          <td><a href="/users/{{ u.id }}">View</a></td>
        </tr>} }}
      </tbody>
    </table>

    <h2>Add User</h2>
    <form method="post">
      <label>
        Name
        <input type="text" name="name" required />
      </label>
      <label>
        Email
        <input type="email" name="email" required />
      </label>
      <button type="submit">Create User</button>
    </form>
  </main>
)
```

### Shared Logic (Instead of Middleware)

There is no middleware in Bialet. For shared logic like authentication or rate limiting, create helper classes in `_app/`:

```wren
// _app/auth.wren

class Auth {
  // Check API key and return the associated account, or null
  static check(apiKey) {
    if (!apiKey) return null
    return `SELECT * FROM api_keys WHERE key = ? AND active = 1`.first(apiKey)
  }

  // Require authentication - returns error response or null
  static require() {
    var apiKey = Request.header("X-Api-Key")
    var account = Auth.check(apiKey)
    if (!account) {
      Response.status(401)
      return Response.json({"error": "Unauthorized"})
    }
    return account
  }
}

class RateLimit {
  // Check rate limit for an API key, returns true if allowed
  static check(apiKey, limit, window) {
    var count = `
      SELECT COUNT(*) FROM requests
      WHERE apiKey = ? AND createdAt > datetime('now', ?)
    `.toNum(apiKey, "-" + window.toString + " seconds")
    return count < limit
  }
}
```

Then use it at the top of your controllers:

```wren
// send.wren - POST /send

import "_app/auth" for Auth, RateLimit
import "_app/domain" for Email

// Authentication check (instead of middleware)
var account = Auth.require()
if (account is String) return account  // Error response

// Rate limiting check
if (!RateLimit.check(account["id"], 100, 3600)) {
  Response.status(429)
  return Response.json({"error": "Rate limit exceeded"})
}

// Main logic
if (!Request.isPost) {
  Response.status(405)
  return Response.json({"error": "Method not allowed"})
}

var to = Request.post("to")
var subject = Request.post("subject")
var body = Request.post("body")

// Validation (inline, no separate validators folder)
if (!to || !to.contains("@")) {
  Response.status(400)
  return Response.json({"error": "Invalid email address"})
}

var email = Email.new()
email.to = to
email.subject = subject
email.body = body
email.accountId = account["id"]
email.save()

return Response.json({"success": true, "id": email.id})
```

### Models (Domain Classes)

Create model classes in `_app/domain.wren` to encapsulate data logic.

Use `.to(Class)` to automatically map query results to domain class instances:

```wren
// _app/domain.wren

class User {
  construct new(data) {
    _id = data["id"]
    _name = data["name"] || ""
    _email = data["email"] || ""
    _createdAt = data["createdAt"]
  }

  // Factory constructor for empty user
  static new() { User.new({}) }

  // Getters (single line)
  id { _id }
  name { _name }
  email { _email }
  createdAt { _createdAt ? Date.new(_createdAt) : null }

  // Setters (single line)
  name=(val) { _name = val.toString.trim() }
  email=(val) { _email = val.toString.trim() }

  // Validation (in the model, not separate validators)
  isValid { _email.contains("@") && _name.count > 0 }

  // Save to database
  save() { _id = `users`.save(this) }

  // Delete from database
  destroy() { `DELETE FROM users WHERE id = ?`.query(_id) }

  // Static query methods using .to(Class) for automatic mapping
  static all() { `SELECT * FROM users ORDER BY name ASC`.fetch.to(User) }
  static find(id) { `SELECT * FROM users WHERE id = ?`.first(id).to(User) }
  static findByEmail(email) { `SELECT * FROM users WHERE email = ?`.first(email).to(User) }
  static count() { `SELECT COUNT(*) FROM users`.toNum }
}
```

### The `.to(Class)` Method

Query results can be automatically mapped to domain classes:

```wren
// Map multiple results (returns List of class instances)
var posts = `SELECT * FROM posts`.fetch.to(Post)

// Map single result (returns single class instance or null)
var user = `SELECT * FROM users WHERE id = ?`.first(1).to(User)

// Works with any query
var recentPosts = `SELECT * FROM posts WHERE createdAt > ? ORDER BY createdAt DESC`.fetch(lastWeek).to(Post)
```

The class must have a `construct new(data)` that accepts a Map.

### Views (Layout Template)

Create a reusable layout in `_app/template.wren`:

```wren
// _app/template.wren

class Layout {
  static render(content) { Layout.render(content, "My App") }

  static render(content, title) { <!doctype html>
    <html lang="en">
      <head>
        <meta charset="utf-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1" />
        <title>{{ title }}</title>
        <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/@picocss/pico@2/css/pico.min.css" />
      </head>
      <body>
        <header class="container">
          <nav>
            <ul>
              <li><strong>My App</strong></li>
            </ul>
            <ul>
              <li><a href="/">Home</a></li>
              <li><a href="/users">Users</a></li>
              <li><a href="/about">About</a></li>
            </ul>
          </nav>
        </header>

        {{ content }}

        <footer class="container">
          <small>Built with <a href="https://bialet.dev">Bialet</a></small>
        </footer>
      </body>
    </html> }
}
```

### Migrations

Define database schema in `_app/migration.wren`:

```wren
// _app/migration.wren

Db.migrate("Create users table", `
  CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    email TEXT UNIQUE NOT NULL,
    password TEXT,
    createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
  )
`)

Db.migrate("Create posts table", `
  CREATE TABLE posts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    content TEXT,
    userId INTEGER REFERENCES users(id),
    published BOOLEAN DEFAULT 0,
    createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
  )
`)

Db.migrate("Add index on posts userId", `
  CREATE INDEX IF NOT EXISTS idx_posts_userId ON posts(userId)
`)
```

## JSON APIs

For API endpoints that return JSON instead of HTML:

```wren
// api/_route.wren - Handles /api/*

import "_app/domain" for User, Post

// Enable CORS for all origins (simplest form)
// This allows the API to be accessed from any web application
if (Response.cors) return

var resource = Request.route(0)  // "users", "posts", etc.
var id = Request.route(1)        // optional ID

// GET /api/users
if (resource == "users" && Request.isGet) {
  var users = User.all()
  return Response.json(users.map{|u| {
    "id": u.id,
    "name": u.name,
    "email": u.email
  }}.toList)
}

// GET /api/users/:id
if (resource == "users" && id && Request.isGet) {
  var user = User.find(id)
  if (!user) {
    Response.status(404)
    return Response.json({"error": "User not found"})
  }
  return Response.json({
    "id": user.id,
    "name": user.name,
    "email": user.email
  })
}

// POST /api/users
if (resource == "users" && Request.isPost) {
  var user = User.new()
  user.name = Request.post("name")
  user.email = Request.post("email")

  if (!user.isValid) {
    Response.status(400)
    return Response.json({"error": "Invalid user data"})
  }

  user.save()
  Response.status(201)
  return Response.json({"id": user.id, "name": user.name})
}

// 404 for unknown routes
Response.status(404)
return Response.json({"error": "Not found"})
```

### CORS (Cross-Origin Resource Sharing)

When building APIs that need to be accessed from web browsers on different domains,
you need to enable CORS. Bialet provides several convenient ways to do this:

```wren
// Simplest form - allow all origins with default settings
if (Response.cors) return

// Allow specific origin with default methods and headers
if (Response.cors("https://myapp.com")) return

// Full control over CORS settings
if (Response.cors("https://myapp.com", "GET, POST, PUT", "Content-Type, X-API-Key")) return
```

The `Response.cors` method automatically handles OPTIONS preflight requests by:
1. Setting the appropriate CORS headers
2. Responding with a 204 No Content status for OPTIONS requests
3. Returning `true` for OPTIONS requests (so you can return early)
4. Returning `false` for other requests (so processing continues)

**Default CORS settings:**
- Methods: `GET, POST, PUT, DELETE, OPTIONS`
- Headers: `Content-Type, Authorization`

**Example with CORS in a full API:**

```wren
// api/_route.wren
import "_app/domain" for User

// Enable CORS for specific frontend domain
if (Response.cors("https://app.example.com")) return

var resource = Request.route(0)
var id = Request.route(1)

if (resource == "users" && Request.method == "GET") {
  var users = `SELECT id, name, email FROM users`.fetch()
  return Response.json(users)
}

Response.status(404)
return Response.json({"error": "Not found"})
```

### Simple API Endpoint

For a single-purpose API endpoint:

```wren
// health.wren - GET /health

return Response.json({
  "status": "ok",
  "timestamp": Date.new().timestamp
})
```

```wren
// send.wren - POST /send (email sending API)

import "_app/auth" for Auth
import "_app/domain" for Email

var account = Auth.require()
if (account is String) return account

if (!Request.isPost) {
  Response.status(405)
  return Response.json({"error": "Method not allowed"})
}

var email = Email.new()
email.to = Request.post("to")
email.subject = Request.post("subject")
email.body = Request.post("body")
email.accountId = account["id"]

if (!email.isValid) {
  Response.status(400)
  return Response.json({"error": "Invalid email data", "details": email.errors})
}

email.save()
return Response.json({"success": true, "id": email.id})
```

## HTML and Styling Guidelines

### Use Semantic HTML with PicoCSS

PicoCSS provides beautiful default styling for semantic HTML elements without classes:

```wren
return Layout.render(
  <main class="container">
    <article>
      <header>
        <h1>{{ post.title }}</h1>
        <small>By {{ post.user.name }} on {{ post.createdAt.format("Y-m-d") }}</small>
      </header>

      <p>{{ post.content }}</p>

      <footer>
        <a href="/posts" role="button" class="secondary">Back to Posts</a>
      </footer>
    </article>
  </main>
)
```

### Semantic HTML Elements

Use proper HTML5 semantic elements:

- `<header>` - Page or section header
- `<nav>` - Navigation links
- `<main>` - Main content (use `class="container"` for centered layout)
- `<article>` - Self-contained content (blog post, comment, card)
- `<section>` - Thematic grouping of content
- `<aside>` - Sidebar content
- `<footer>` - Page or section footer
- `<details>` / `<summary>` - Expandable content

### Forms with PicoCSS

```wren
<form method="post">
  <label>
    Username
    <input type="text" name="username" placeholder="Enter username" required />
  </label>

  <label>
    Email
    <input type="email" name="email" placeholder="Enter email" required />
  </label>

  <label>
    Password
    <input type="password" name="password" placeholder="Enter password" required />
  </label>

  <label>
    <input type="checkbox" name="remember" />
    Remember me
  </label>

  <label>
    Role
    <select name="role">
      <option value="user">User</option>
      <option value="admin">Admin</option>
    </select>
  </label>

  <label>
    Bio
    <textarea name="bio" placeholder="Tell us about yourself"></textarea>
  </label>

  <button type="submit">Create Account</button>
</form>
```

### Tables

```wren
<table>
  <thead>
    <tr>
      <th scope="col">Name</th>
      <th scope="col">Email</th>
      <th scope="col">Role</th>
      <th scope="col">Actions</th>
    </tr>
  </thead>
  <tbody>
    {{ users.map{|u| <tr>
      <td>{{ u.name }}</td>
      <td>{{ u.email }}</td>
      <td>{{ u.role }}</td>
      <td>
        <a href="/users/{{ u.id }}">Edit</a>
      </td>
    </tr>} }}
  </tbody>
</table>
```

### Cards with Article

```wren
<div class="grid">
  {{ posts.map{|p| <article>
    <header>
      <h3>{{ p.title }}</h3>
    </header>
    <p>{{ p.content.take(150) }}...</p>
    <footer>
      <a href="/posts/{{ p.id }}" role="button" class="outline">Read more</a>
    </footer>
  </article>} }}
</div>
```

## Database Operations

### Direct SQL Queries (No ORM)

Always use parameterized queries with `?` placeholders:

```wren
// Fetch multiple rows
var users = `SELECT * FROM users WHERE active = 1`.fetch

// Fetch multiple rows as class instances
var users = `SELECT * FROM users WHERE active = 1`.fetch.to(User)

// Fetch first row
var user = `SELECT * FROM users WHERE id = ?`.first(userId)

// Fetch first row as class instance
var user = `SELECT * FROM users WHERE id = ?`.first(userId).to(User)

// Get single value
var count = `SELECT COUNT(*) FROM users`.toNum
var name = `SELECT name FROM users WHERE id = ?`.val(userId)

// Insert
var id = `INSERT INTO users (name, email) VALUES (?, ?)`.query(name, email)

// Update
`UPDATE users SET name = ?, email = ? WHERE id = ?`.query(name, email, id)

// Delete
`DELETE FROM users WHERE id = ?`.query(id)

// Using `.save()` on table queries for insert/update
var userData = {"name": "John", "email": "john@example.com"}
var id = `users`.save(userData)

// Update existing record
userData["id"] = id
userData["name"] = "John Doe"
`users`.save(userData)
```

### Query Methods

- `.query(params)` - Execute query, returns last insert ID for INSERT
- `.fetch(params)` - Returns array of rows (List of Maps)
- `.first(params)` - Returns first row (Map) with automatic LIMIT 1
- `.val(params)` - Returns first value of first row
- `.toNum(params)` - Returns first value as number
- `.to(Class)` - Maps results to class instances (works with `.fetch` and `.first`)

## Request Handling

### GET Parameters

```wren
var id = Request.get("id")
var page = Request.get("page") || "1"
var search = Request.get("q")
```

### POST Parameters

```wren
if (Request.isPost) {
  var name = Request.post("name")
  var email = Request.post("email")
  // Process form...
  return Response.redirect("/success")
}
```

### Headers

```wren
var apiKey = Request.header("X-Api-Key")
var contentType = Request.header("Content-Type")
```

### HTTP Methods

```wren
Request.isGet     // true for GET requests
Request.isPost    // true for POST requests
Request.method    // "GET", "POST", "PUT", "DELETE", etc.
```

### Dynamic Routes

Use `_route.wren` for dynamic URL segments:

```wren
// users/_route.wren
// Handles /users/:id

import "_app/template" for Layout
import "_app/domain" for User

var userId = Request.route(0)

if (!userId) return Response.redirect("/users")

var user = User.find(userId)

if (!user) {
  Response.status(404)
  return Layout.render(<main class="container"><h1>User not found</h1></main>)
}

// Handle update
if (Request.isPost) {
  user.name = Request.post("name")
  user.email = Request.post("email")
  user.save()
  return Response.redirect("/users/" + userId)
}

return Layout.render(
  <main class="container">
    <h1>{{ user.name }}</h1>
    <form method="post">
      <label>
        Name
        <input type="text" name="name" value="{{ user.name }}" required />
      </label>
      <label>
        Email
        <input type="email" name="email" value="{{ user.email }}" required />
      </label>
      <button type="submit">Update</button>
    </form>
  </main>
)
```

## Session and Authentication

```wren
// Login example
if (Request.isPost) {
  var email = Request.post("email")
  var password = Request.post("password")

  var user = `SELECT * FROM users WHERE email = ?`.first(email)

  if (user && Util.verify(password, user["password"])) {
    Session["userId"] = user["id"]
    Session["userName"] = user["name"]
    return Response.redirect("/dashboard")
  }

  var error = "Invalid email or password"
}

// Check if logged in (in other pages)
if (!Session["userId"]) return Response.redirect("/login")

// Logout
Session.destroy()
return Response.redirect("/")
```

## HTMX Integration (Optional)

When you need dynamic updates without full page reloads, use HTMX sparingly:

```wren
// In your layout, add HTMX
<script src="https://unpkg.com/htmx.org@1.9.10"></script>

// Delete button with confirmation
<button
  hx-delete="/users/{{ user.id }}"
  hx-confirm="Are you sure?"
  hx-target="closest tr"
  hx-swap="outerHTML">
  Delete
</button>

// Load more content
<button
  hx-get="/posts?page={{ nextPage }}"
  hx-target="#posts-list"
  hx-swap="beforeend">
  Load More
</button>

// Inline editing
<span
  hx-get="/users/{{ user.id }}/edit"
  hx-trigger="click"
  hx-swap="outerHTML">
  {{ user.name }}
</span>
```

### HTMX Endpoint Example

```wren
// users/_route.wren
var userId = Request.route(0)
var action = Request.route(1)

if (action == "edit") {
  var user = User.find(userId)
  return <form hx-put="/users/{{ userId }}" hx-swap="outerHTML">
    <input type="text" name="name" value="{{ user.name }}" />
    <button type="submit">Save</button>
  </form>
}

// Handle PUT for HTMX
if (Request.method == "PUT") {
  var user = User.find(userId)
  user.name = Request.post("name")
  user.save()
  return <span hx-get="/users/{{ userId }}/edit" hx-trigger="click" hx-swap="outerHTML">{{ user.name }}</span>
}
```

## Best Practices

### 1. Keep Controllers Simple
Put business logic in domain classes, not in page files.

### 2. Use Semantic HTML
Let PicoCSS handle styling through proper HTML structure.

### 3. Direct SQL is Fine
Write clear, readable SQL queries. Avoid complex abstractions.

### 4. Full Page Reloads by Default
Only use HTMX when it genuinely improves user experience.

### 5. Validate Input
Always validate and sanitize user input in domain classes:

```wren
class User {
  // ... properties ...

  isValid { _email.contains("@") && _name.count > 0 }

  errors {
    var e = []
    if (!_email.contains("@")) e.add("Invalid email")
    if (_name.count == 0) e.add("Name is required")
    return e
  }
}
```

### 6. Use Prepared Statements
Never concatenate user input into SQL queries:

```wren
// WRONG - SQL injection vulnerability
`SELECT * FROM users WHERE name = '%(name)'`.fetch

// CORRECT - Use parameterized queries
`SELECT * FROM users WHERE name = ?`.fetch(name)
```

### 7. Escape Output
Use `.safe` for user-generated content in HTML:

```wren
<p>{{ user.bio.safe }}</p>
```

### 8. Single-Line Methods
Keep simple methods on one line to leverage implicit returns:

```wren
// Good
static all() { `SELECT * FROM users`.fetch.to(User) }
save() { _id = `users`.save(this) }
name { _name }

// Avoid unless complex logic requires multiple lines
static all() {
  return `SELECT * FROM users`.fetch.to(User)
}
```

### 9. No Unnecessary Folders
Don't create `routes/`, `validators/`, `middleware/`, or `static/` folders. Keep it simple.

## Complete Example: Email API Service

```
email-api/
├── _app/
│   ├── migration.wren
│   ├── domain.wren
│   └── auth.wren
├── .gitignore
├── index.wren            # GET / - Documentation page
├── send.wren             # POST /send - Send email
├── templates.wren        # GET /templates - List templates
├── template/
│   └── _route.wren       # GET/POST /template/:name
└── health.wren           # GET /health - Health check
```

### .gitignore
```text
_db.sqlite3*
```

### _app/migration.wren
```wren
Db.migrate("Create api_keys table", `
  CREATE TABLE api_keys (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    key TEXT UNIQUE NOT NULL,
    name TEXT NOT NULL,
    active BOOLEAN DEFAULT 1,
    createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
  )
`)

Db.migrate("Create emails table", `
  CREATE TABLE emails (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    apiKeyId INTEGER REFERENCES api_keys(id),
    toAddress TEXT NOT NULL,
    subject TEXT NOT NULL,
    body TEXT,
    status TEXT DEFAULT 'pending',
    createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
  )
`)

Db.migrate("Create templates table", `
  CREATE TABLE templates (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL,
    subject TEXT NOT NULL,
    body TEXT NOT NULL,
    variables TEXT,
    createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
  )
`)
```

### _app/domain.wren
```wren
class Email {
  construct new(data) {
    _id = data["id"]
    _apiKeyId = data["apiKeyId"]
    _to = data["toAddress"] || ""
    _subject = data["subject"] || ""
    _body = data["body"] || ""
    _status = data["status"] || "pending"
    _createdAt = data["createdAt"]
  }

  static new() { Email.new({}) }

  id { _id }
  to { _to }
  subject { _subject }
  body { _body }
  status { _status }
  apiKeyId { _apiKeyId }

  to=(val) { _to = val.toString.trim() }
  subject=(val) { _subject = val.toString.trim() }
  body=(val) { _body = val }
  apiKeyId=(val) { _apiKeyId = val }

  isValid { _to.contains("@") && _subject.count > 0 }

  errors {
    var e = []
    if (!_to.contains("@")) e.add("Invalid email address")
    if (_subject.count == 0) e.add("Subject is required")
    return e
  }

  save() { _id = `emails`.save(this) }

  static find(id) { `SELECT * FROM emails WHERE id = ?`.first(id).to(Email) }
  static pending() { `SELECT * FROM emails WHERE status = 'pending'`.fetch.to(Email) }
}

class Template {
  construct new(data) {
    _id = data["id"]
    _name = data["name"] || ""
    _subject = data["subject"] || ""
    _body = data["body"] || ""
    _variables = data["variables"] || ""
  }

  static new() { Template.new({}) }

  id { _id }
  name { _name }
  subject { _subject }
  body { _body }
  variables { _variables.split(",").map{|v| v.trim()}.toList }

  name=(val) { _name = val.toString.trim() }
  subject=(val) { _subject = val.toString.trim() }
  body=(val) { _body = val }
  variables=(val) { _variables = val is List ? val.join(",") : val }

  save() { _id = `templates`.save(this) }

  static all() { `SELECT * FROM templates ORDER BY name`.fetch.to(Template) }
  static find(id) { `SELECT * FROM templates WHERE id = ?`.first(id).to(Template) }
  static findByName(name) { `SELECT * FROM templates WHERE name = ?`.first(name).to(Template) }
}
```

### _app/auth.wren
```wren
class Auth {
  static check(apiKey) {
    if (!apiKey) return null
    return `SELECT * FROM api_keys WHERE key = ? AND active = 1`.first(apiKey)
  }

  static require() {
    var apiKey = Request.header("X-Api-Key")
    var account = Auth.check(apiKey)
    if (!account) {
      Response.status(401)
      return Response.json({"error": "Unauthorized"})
    }
    return account
  }
}
```

### send.wren
```wren
import "_app/auth" for Auth
import "_app/domain" for Email

var account = Auth.require()
if (account is String) return account

if (!Request.isPost) {
  Response.status(405)
  return Response.json({"error": "Method not allowed"})
}

var email = Email.new()
email.to = Request.post("to")
email.subject = Request.post("subject")
email.body = Request.post("body")
email.apiKeyId = account["id"]

if (!email.isValid) {
  Response.status(400)
  return Response.json({"error": "Validation failed", "details": email.errors})
}

email.save()
return Response.json({"success": true, "id": email.id})
```

### health.wren
```wren
return Response.json({
  "status": "ok",
  "timestamp": Date.new().timestamp
})
```

---

## Summary

When building Bialet applications:

1. **Code Style**: 2-space indentation, single-line methods when possible
2. **Structure**: Use `_app/` folder for template, migrations, and domain classes
3. **No Extra Folders**: No `routes/`, `validators/`, `middleware/`, `static/`
4. **Controllers**: Each `.wren` file IS a route with logic at top, view at bottom
5. **Models**: Domain classes with validation, `.to(Class)` for query mapping
6. **Shared Logic**: Helper classes in `_app/` instead of middleware
7. **Database**: Direct SQL with parameterized queries in `_app/migration.wren`
8. **APIs**: Use `Response.json()` for JSON responses
9. **Routing**: File-based - the file path IS the URL path
10. **HTML**: Semantic markup with PicoCSS, minimal JavaScript
11. **Git**: Always add `_db.sqlite3*` to `.gitignore`
