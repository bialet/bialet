# Testing in Bialet

Bialet includes a built-in test runner. A test is a Wren file that makes a
request and asserts the response. The request goes through your real router, so
the test exercises the same code a browser would hit.

## Running Tests

Use the `-T` flag to run all tests:

```bash
bialet -T                    # Run all tests in _tests/ folder
bialet -T docs/examples      # Run tests in specific directory
bialet -T -q                 # Quiet mode: no colors, minimal output (for CI)
```

## Writing Your First Test

A test file is a plain Wren script. Statements run top to bottom; there are no
test functions and no registration step. Each `Test.get` / `Test.post` call
builds a request, runs it in a fresh VM through the real router, and returns an
object you assert on.

**Application file:** `hi.wren`

```wren
return <h1>👋 Hello World</h1>
```

**Test file:** `_tests/hi.wren`

```wren
Test.get("/hi")
    .status(200)
    .contains("Hello World")
```

**Run it:**

```bash
$ bialet -T
Running tests in _tests...

  ✓ hi.wren (5ms)

Summary:

Total Tests: 1
Passed Tests: 1
Failed Tests: 0
Skipped Tests: 0
Elapsed Time: 5ms
```

A failed assertion aborts the file and reports the assertion message:

```bash
$ bialet -T
Running tests in _tests...

  ✗ hi.wren
      Expected code to be 200 but was 500

Summary:

Total Tests: 1
Passed Tests: 0
Failed Tests: 1
Skipped Tests: 0
Elapsed Time: 20ms
```

The first failed assertion ends the file, so assertions after it do not run.
Keep tests in a file independent of each other, or split them.

### Where the test goes

Mirror your route files. A route in `hi.wren` gets `_tests/hi.wren`; a todo app
in `tasks.wren` gets `_tests/tasks.wren`. The directory name is all the runner
needs — any `_tests/*.wren` file runs, in any order.

### Arrange, act, assert

Most tests have three beats: set up, perform the action, check the result.
Assert the flow the user asked for, not just that a page renders:

```wren
// _tests/tasks.wren

// Arrange: the table starts empty in this file's database state.
Test.get("/tasks").status(200).notContains("Buy bread")

// Act: the form POST writes the row.
Test.post("/tasks", {"title": "Buy bread"}).status(302)

// Assert: the next GET reads the same value back.
Test.get("/tasks").status(200).contains("Buy bread")
```

A page that answers 200 with an empty list proves nothing. The POST that
redirects and the GET that shows the new row together prove the feature works.

## Test Directory Structure

Tests live in `_tests/` inside your application:

```
myapp/
├── _app.wren
├── _migration.wren
├── hi.wren
├── tasks.wren
├── _app/
│   ├── components/
│   │   ├── layout.wren
│   │   └── cards.wren
│   └── domain.wren
├── _tests/
│   ├── _init.wren           # Optional: runs before each test file
│   ├── hi.wren              # Tests for hi.wren
│   └── tasks.wren           # Tests for tasks.wren
```

**Important:** the `_tests/` folder name starts with `_` to prevent direct HTTP
access, following Bialet's convention for private files.

## Test Initialization

### `_tests/_init.wren` (Optional)

If present, this file runs before every test file, inside that file's
savepoint. Use it for shared setup:

```wren
// _tests/_init.wren
System.log("Setting up test environment")

// Insert test data
`INSERT INTO users (name, email) VALUES (?, ?)`.query("Test User", "test@example.com")
```

Two consequences:

- Top-level `var`s declared here are visible to every test file, because the
  file runs in the same module as the test.
- Rows written here are visible to the test file that follows and gone once that
  file finishes. Each file gets a fresh copy of the same seeded state.

## Skipping Tests

Call `Tests.skip()` as a test file's first statement to mark it skipped
instead of run. Skipped tests count separately from passed/failed and never
affect the exit code:

```wren
// _tests/maintenance.wren
Tests.skip()
```

`Tests.skip()` doesn't abort the fiber, so any code after it in the same file
still executes -- put it first if the rest of the file should not run.

## Writing Tests

Tests use the `Test` class with a fluent API:

```wren
// Basic GET request test
Test.get("/hi").status(200).contains("Hello World")

// POST request test with form data
Test.post("/password", {"password":"123", "password-check":"123"})
    .status(200)
    .contains("The passwords are the same")
```

Requests are lazy: nothing runs until the first assertion (or `.json()`,
`.csrf()` or `.text()`) asks for the response. Chain as many assertions as you
need on one request.

## Test API Reference

### HTTP Request Methods

| Method | Description |
|--------|-------------|
| `Test.get(route)` | Send GET request |
| `Test.post(route, params)` | Send POST request with form data |
| `Test.apiGet(route)` | Send GET with JSON Content-Type header |
| `Test.apiPost(route, params)` | Send POST with JSON body |
| `Test.apiPut(route, params)` | Send PUT with JSON body |
| `Test.apiDelete(route)` | Send DELETE request |

### Request Configuration

Chain these before assertions:

```wren
Test.get("/api/user")
    .setHeader("Authorization", "Bearer token123")  // Add custom header
    .method("GET")                                     // Explicitly set method
    .postData({"key": "value"})                       // Set POST data
    .withCsrf()                                        // Add the form's CSRF token
```

### Cookies and Sessions

The `Test` client keeps a cookie jar shared by every request in a test file, so
a session minted by one call is the session the next call presents:

```wren
var page = Test.get("/login").status(200)
var token = page.csrf()      // hidden _bialet_csrf value from the form
Test.post("/login", {
  "email": "user@example.com",
  "password": "secret",
  "_bialet_csrf": token
}).status(302)
```

`Test.cookie(name)` reads a stored cookie and `Test.clearCookies()` empties the
jar. `.csrf()` aborts the test when the response rendered no token. Session
values set during a `Test.get` are therefore visible to the following
`Test.post`, and a CSRF-protected form validates without a `test_mode` flag.

### CSRF-Protected Forms

A protected form takes two requests on one session: a GET that renders the
token the server stored, and a POST that sends it back. The cookie jar already
shares the session, and `.withCsrf()` folds the two calls into one:

```wren
// The form is rendered by the route being posted to: GET it for the token.
Test.post("/login", {"email": "user@example.com"})
    .withCsrf()
    .status(302)

// The form lives on another route.
Test.post("/login", {"email": "user@example.com"})
    .withCsrf("/login")
    .status(302)

// The page was already fetched, so reuse it instead of fetching it twice.
var page = Test.get("/signup").status(200)
Test.post("/signup", {"email": "user@example.com"})
    .withCsrf(page)
    .status(201)
```

`.withCsrf()` adds `_bialet_csrf` to the form Map just before the request runs,
so the request is still lazy: nothing is fetched until the first assertion (or
`.csrf()`) runs. The source may be a route string, which is fetched as a GET
with the caller's headers copied so an explicit `Cookie` header keeps both
requests on the same session, or an earlier `Test` / `TestResponse`. With no
argument the route is the one being posted to.

`.withCsrf()` needs form data (a Map). `Test.apiPost` and `Test.apiPut` send a
raw JSON string and a multipart body is assembled by the client, so neither has
a place to add a form field and the call aborts with `withCsrf() needs form data
(a Map), not a raw body`. Add the token yourself in those flows:

```wren
var page = Test.get("/form").status(200)
Test.apiPost("/api/form", {"_bialet_csrf": page.csrf()})
```

`.csrf()` remains for manual flows: it runs the request if needed and returns
the hidden `_bialet_csrf` value from the body, aborting when the page rendered
none.

### Assertions

Chain these after the request method:

| Assertion | Description |
|-----------|-------------|
| `.status(code)` | Assert HTTP status code |
| `.contains(str)` | Assert body contains string |
| `.equals(str)` | Assert body exactly equals string |
| `.notContains(str)` | Assert body doesn't contain string |
| `.textContains(str)` | Assert visible text (tags stripped, whitespace collapsed) contains string |
| `.text()` | Return visible text (tags stripped, whitespace collapsed) |
| `.header(name)` | Assert header exists |
| `.header(name, value)` | Assert header equals value |
| `.json()` | Return parsed JSON body (Map or List) |
| `.jsonContains(key)` | Assert JSON object has key |
| `.jsonEquals(key, value)` | Assert JSON key equals value |
| `.jsonNotContains(key)` | Assert JSON object doesn't have key |
| `.assert(condition, message)` | Custom assertion with message |

**Note:** Assertions are lazy - the request is executed when the first assertion is called.

`.contains()` matches raw markup; attributes and script bodies can satisfy it. Use
`.textContains()` for visible text: it strips tags and collapses whitespace, so a
multi-word assertion does not depend on how the template wraps the line.

> ⚠️ Pitfall: there is no `Test.headers` map. Use the `.header(name)` assertion
> to check a header exists, `.header(name, value)` to check its value, and
> `.text()` / `.json()` for the body.

### Body Assertions

```wren
// Exact match
Test.get("/health").status(200).equals("OK")

// Contains substring
Test.get("/page").contains("Welcome")

// Negative assertion (security checks)
Test.get("/public").notContains("password")

// Multiple assertions chained
Test.get("/api")
    .status(200)
    .contains("success")
    .notContains("error")
```

### JSON Assertions

For API endpoints that return JSON:

```wren
// Parse JSON body for custom validation
var data = Test.apiGet("/users").status(200).json()
Test.assert(data.count > 0, "Expected at least one user")

// Fluent JSON assertions
Test.apiGet("/api-user")
    .status(200)
    .jsonContains("id")
    .jsonContains("name")
    .jsonContains("email")
    .jsonEquals("id", 1)
    .jsonEquals("name", "Test User")
    .jsonEquals("active", true)
    .jsonNotContains("password")
    .jsonNotContains("ssn")
```

### Two Approaches to JSON Testing

**Approach 1: Fluent JSON methods** (best for simple checks)

```wren
Test.apiGet("/api-user")
    .jsonContains("id")
    .jsonEquals("name", "Test User")
    .jsonNotContains("password")
```

**Approach 2: Parse JSON + custom logic** (best for complex validation)

```wren
var counters = Test.apiGet("/json").json()
Test.assert(counters.count >= 2, "Expected at least 2 counters")

for (counter in counters) {
  if (counter["name"] == "test1") {
    Test.assert(counter["value"] == "10", "Expected value '10'")
  }
}
```

### Multiple Assertions

Chain multiple assertions on a single request:

```wren
Test.get("/api/users")
    .status(200)
    .header("Content-Type", "application/json")
    .contains("user@example.com")
```

### Multiple Tests in One File

Write multiple independent tests in a single file:

```wren
// _tests/api.wren

// Test 1: List users
Test.get("/api/users").status(200).contains("users")

// Test 2: Create user
Test.apiPost("/api/users", {"name": "John"})
    .status(201)
    .contains("John")

// Test 3: Invalid request
Test.post("/api/users", {})
    .status(400)
    .contains("name is required")
```

## Testing Components and Domain Classes

`Test.get` exercises the app from the outside: real request in, response out.
That is the default, and it is what most tests need.

Some behaviour is easier to pin down without an HTTP round trip: a component
that must escape a value, a domain query that must scope rows to the current
session, a formatter with edge cases. A test file is its own Wren module, so it
can import the app code directly and call it:

```wren
// _app/components/cards.wren
class Cards {
  static tool(item) { <article class="card tool">
    <h3>{{ item["name"] }}</h3>
    <p class="meta">Wear {{ item["wear"] }} of 100</p>
  </article> }
}
```

```wren
// _tests/components.wren
import "../_app/components/cards" for Cards

var markup = Cards.tool({"name": "Hammer", "wear": "40"}).toString

Test.assert(markup.contains("Hammer"), "the card renders its name")
Test.assert(markup.contains("Wear 40 of 100"), "it renders the wear")

var escaped = Cards.tool({"name": "<b>", "wear": "1"}).toString
Test.assert(escaped.contains("&lt;b&gt;"), "the name is escaped")
```

A component returns `HtmlNode`, not a String: call `.toString` before you use
`String` methods on it. `HtmlNode.contains` compares characters and never
matches a substring.

Both import forms work:

```wren
import "../_app/components/cards" for Cards   // relative to _tests/
import "/_app/components/cards" for Cards     // root-absolute
```

Use whichever form your route files use. Root-absolute imports resolve from the
app root and survive moving the test; `../_app/...` is shorter and reads well
next to the module it exercises.

| Test style | Reach for it when |
|---|---|
| Black-box (`Test.get`) | You care that the flow works end to end: form posts, redirects, session, rendered output |
| White-box (import + `Fixture`) | You care about one function or one component's output, and building a route to reach it would be noise |

White-box tests skip the router. They do not prove the route is wired up, so
keep a black-box test for every feature that matters and use the import when the
unit is hard to reach from outside.

### Components

Components are plain classes with methods that return HTML — see
[Template Classes / Components](template.md). Call them like any other method:

```wren
// _app/components/layout.wren
class Layout {
  static page(title, user, content) { <!doctype html>
    <html lang="en">
      <head><title>{{ title }}</title></head>
      <body>
        {{ user && <span class="who">{{ user["name"] }}</span> }}
        <main>{{ content }}</main>
      </body>
    </html> }
}
```

```wren
// _tests/layout.wren
import "../_app/components/layout" for Layout

var content = <h1>Tasks</h1>
var page = Layout.page("Tasks", null, content).toString

Test.assert(page.contains("<title>Tasks</title>"), "the title reaches the head")
Test.assert(page.contains("<h1>Tasks</h1>"), "the content lands in main")
```

`Layout.page` takes `title`, `user`, `content`, in that order. Pass `null` for
`user` when there is none, and build `content` in a variable first: an inline
HTML literal cannot start right after a comma, so
`Layout.page("Tasks", null, <h1>Tasks</h1>)` does not parse.

| Class | Methods |
|---|---|
| `Cards` | `Cards.tool(item)`, `Cards.mission(row)` |
| `Hud` | `Hud.state(game)` |
| `Nav` | `Nav.bar(user)` |
| `Layout` | `Layout.page(title, user, content)` |

One class per file under `_app/components/`. See
[Template Classes / Components](template.md) for the full set.

### Domain classes that read `Session`

A domain method that filters on `Session.id` needs a request behind it before
it can run. `Fixture` builds one without going through the router. Apply the
fixture, then call the imported code:

```wren
// _app/domain.wren
class Task {
  static mine() { `
    SELECT * FROM tasks WHERE session = ? ORDER BY createdAt ASC
  `.fetch(Session.id).to(Task) }
}
```

```wren
// _tests/domain.wren
import "../_app/domain" for Task

var fixture = Fixture.get("/tasks").apply()

// The fixture's request is now the current request: Session.id resolves.
`INSERT INTO tasks (title, session) VALUES (?, ?)`.query("Buy bread", Session.id)

var tasks = Task.mine()
Test.assert(tasks.count == 1, "the domain query scopes rows to the session")
Test.assert(tasks[0].title == "Buy bread", "it returns the seeded row")
```

Calling code that reads `Request`, `Cookie` or `Session` directly from a test
fails at the first access unless a request is installed. `Fixture` is what
installs one without the router.

## The `Fixture` Class

`Fixture` fakes a request in place of the router. It builds the request from a
fluent chain, installs it with `apply()`, and afterwards exposes what the code
under test set on the response.

```wren
var fixture = Fixture.post("/tasks", {"title": "Buy bread"}).apply()

// The controller code under test, lifted out of its route:
`INSERT INTO tasks (title, session) VALUES (?, ?)`
    .query(Request.post("title"), Session.id)
Response.redirect("/tasks")

Test.assert(fixture.status == 302, "the POST redirects")
Test.assert(fixture.header("location") == "/tasks", "it redirects to the list")
```

`apply()` calls `Response.init` and `Request.init` and returns the fixture, so
the chain and the assertions can share one expression:

```wren
var page = Fixture.get("/tasks").query("page", "2").apply()
Test.assert(page.status == 200, "the page renders")
```

| Member | Description |
|---|---|
| `Fixture.get(path)` | Build a GET fixture |
| `Fixture.post(path, form)` | Build a POST fixture with a form Map |
| `Fixture.put(path, form)` | Build a PUT fixture with a form Map |
| `Fixture.patch(path, form)` | Build a PATCH fixture with a form Map |
| `Fixture.delete(path)` | Build a DELETE fixture |
| `Fixture.request(method, path)` | Build a fixture for any method |
| `.route(prefix)` | Matched route prefix, so `Request.route(0)` reads the segment after it (`.route("/notes")` for `/notes/42`) |
| `.query(name, value)` | Add a query-string parameter |
| `.post(name, value)` | Add a form field |
| `.body(rawString)` | Set a raw request body |
| `.header(name, value)` | Add a request header |
| `.cookie(name, value)` | Add a request cookie |
| `.fields(encoded)` | Multipart form fields, already encoded |
| `.files(encoded)` | Multipart file parts, already encoded |
| `.apply()` | Install the request (`Response.init` + `Request.init`); returns the fixture |
| `.status` | Response status set by the code under test |
| `.body` | Response body |
| `.headers` | Response headers, a Map with lowercased names |
| `.header(name)` | One response header value |

`.body` has two roles. With an argument it sets a raw request body; with none it
reads the response body back after `apply()`. `.body(rawString)` and
`.post(name, value)` cover the two common bodies — reach for `.fields` /
`.files` only when a test needs a real multipart payload.

> ⚠️ Pitfall: `Fixture` does not run the router and does not follow redirects.
> A handler that calls `Response.redirect` leaves `fixture.status == 302` and a
> `location` header for you to assert; it does not load the target page. Build a
> second fixture for the redirect target, or use `Test.get` / `Test.post`.

> ⚠️ Pitfall: there are no assertion methods on `Fixture`. Compare `status`,
> `body` and `header(...)` yourself with `Test.assert(condition, message)`.

(stubbing-outbound-http-calls)=

## Stubbing Outbound HTTP Calls

A route that calls a third-party API must not hit it during a test. `Test.stubHttp`
replaces the response for matching URLs. Stubs are process-wide, so they apply to
`Http.get` inside a route handler just as they do to a call from the test file:

```wren
// checkout.wren
if (Request.isPost) {
  var charge = Http.post("https://api.example.com/charge",
                         {"amount": Request.post("amount") || "0"})
  if (!charge) {
    Response.status(502)
    return "payment failed"
  }
  return Response.redirect("/thanks")
}
return <form method="post">...</form>
```

```wren
// _tests/checkout.wren
Test.stubHttp("https://api.example.com/charge", {
  "status": 200,
  "headers": {"Content-Type": "application/json"},
  "body": "{\"ok\":true,\"id\":\"ch_1\"}"
})

Test.post("/checkout", {"amount": "1999"}).status(302)

Test.assert(Test.httpCalls.count == 1, "the handler called the API once")
var call = Test.httpCalls[0]
Test.assert(call["method"] == "POST", "it went out as a POST")
Test.assert(call["body"].contains("1999"), "the amount was sent")

Test.clearHttpStubs()
```

The response Map accepts:

| Key | Type | Description |
|---|---|---|
| `status` | Num | Response status, default `200` |
| `headers` | Map | Response headers |
| `body` | String | Response body |
| `error` | Num | Transport error code; simulates a failed call |
| `errorMessage` | String | Error text reported through `Http.errorMessage` |

Setting `error` makes the call behave like a real transport failure: `Http.get`
returns `null` and `Http.error` is non-zero. That is how you test the `if (!charge)`
branch above.

### Patterns and call records

`pattern` is the full URL, with `*` as a wildcard:

```wren
Test.stubHttp("https://api.example.com/*", {...})        // every path on the host
Test.stubHttp("https://api.example.com/charge", {...})   // exactly this URL
Test.stubHttp("https://api.example.com/*/refund", {...}) // any prefix, then /refund
```

The first registered match wins. Register the specific stub before the broad one,
and call `Test.clearHttpStubs()` before the file ends so a later file does not
inherit them.

`Test.httpCalls` records the matched calls in order:

| Key | Description |
|---|---|
| `url` | The full URL that was requested |
| `method` | The HTTP method |
| `headers` | Request headers, Map with lowercased names |
| `body` | The request body as sent |

Only matched calls are recorded. An unmatched URL is not stubbed, so it goes to
the real network with the real timeout — slow tests, flaky failures, and a live
third party receiving your test data.

> ⚠️ Pitfall: an unmatched URL still hits the network. If a test is slow or
> fails only in CI, check the stub pattern first. A pattern needs the full URL,
> scheme included; `"api.example.com/*"` does not match anything.
>
> ⚠️ Pitfall: a redirect is still followed above a stub. The target needs its
> own stub, or it reaches the network.

## Deterministic Time and Randomness

Two test helpers remove the usual sources of flakiness. Both are process-wide,
so route handlers see them too, and the runner resets both before each test
file.

### Freezing time

```wren
Test.freezeTime(1767225600)               // epoch seconds
Test.freezeTime(Date.new(2026, 1, 1))     // a Date
Test.freezeTime("2026-01-01 08:00:00")    // a "YYYY-MM-DD HH:MM:SS" string

Test.get("/reports/2026-01").status(200).contains("January")

Test.unfreezeTime()                       // or Test.freezeTime(null)
```

`Date` reports the frozen instant, and `Cron` decides against it. Set it at the
top of the file that needs it, so the frozen clock sits next to the assertions
it explains.

> ⚠️ Pitfall: `freezeTime` does not touch SQLite. `datetime('now')`,
> `strftime('%Y', 'now')` and `CURRENT_TIMESTAMP` keep returning the real clock
> time. If a query compares against `datetime('now')`, pass the frozen value as
> a parameter instead, or assert against the real date.

### Seeding randomness

```wren
Test.randomSeed(7)
var token = Util.randomString(32)   // same value on every run

Test.randomSeed()                   // restore the OS CSPRNG
```

`randomSeed` makes `Util.randomString` — and the session ids, CSRF tokens and
tokens built on it — repeatable. Use it when a test needs to know a generated
value in advance. Call `Test.randomSeed()` with no argument to restore the OS
CSPRNG.

(database-isolation)=

## Database Isolation

`bialet -T` never touches your development database. It creates one temporary
database file for the run and rolls each test file back when it finishes:

1. A temporary database file is created.
2. Migrations run once, before any test file (`_migration.wren` or
   `_app/migration.wren`).
3. For each test file: a savepoint is opened, `_tests/_init.wren` runs inside
   it, the file runs, and the savepoint is rolled back.
4. The temporary database is deleted when the run ends.

So each test file starts from the same migrated state plus whatever
`_init.wren` seeds, and anything the file writes is gone when it finishes. Data
does not leak between files, and file order does not matter.

Read rows you inserted in the same file: a second file cannot see them, by
design.

> ⚠️ Pitfall: do not run `VACUUM` or `PRAGMA journal_mode` in a test file. Both
> rewrite the database file outside the savepoint, which either fails or leaves
> the file in a state the rollback cannot undo.

> ⚠️ Pitfall: do not leave a transaction open. A `BEGIN` without a matching
> `COMMIT` or `ROLLBACK` leaves the savepoint stack dirty, and the runner's
> rollback at the end of the file has nothing clean to close.

## Best Practices

### Use Appropriate Assertion Methods

```wren
// ✅ Good - specific assertions
Test.get("/health").equals("OK")
Test.apiGet("/users").jsonContains("id")

// ❌ Avoid - too generic
Test.get("/health").contains("OK")  // Could match "NOT OK"
```

### Security Testing

Always check that sensitive data isn't exposed:

```wren
// Ensure passwords/keys not in responses
Test.get("/public/user")
    .notContains("password")
    .jsonNotContains("ssn")
    .jsonNotContains("api_key")
```

### Use Custom Assertions for Complex Logic

```wren
// Instead of manual Fiber.abort()
var data = Test.apiGet("/stats").json()
Test.assert(data["active"] > data["inactive"], "More active expected")
Test.assert(data["total"] == data["active"] + data["inactive"], "Count mismatch")
```

### Chain Related Assertions

```wren
// Group related checks together
Test.apiPost("/users", {"name": "Alice"})
    .status(201)
    .jsonContains("id")
    .jsonContains("createdAt")
    .jsonEquals("name", "Alice")
    .jsonEquals("active", true)
```

### Test Both Success and Failure Cases

```wren
// Success case
Test.post("/login", {"user": "admin", "pass": "correct"})
    .status(200)
    .contains("Welcome")

// Failure case
Test.post("/login", {"user": "admin", "pass": "wrong"})
    .status(401)
    .contains("Invalid credentials")
```

### Test the flow, not the page

```wren
// Weak: the page exists
Test.get("/tasks").status(200)

// Strong: the write is visible on the next read
Test.post("/tasks", {"title": "Buy bread"}).status(302)
Test.get("/tasks").contains("Buy bread")
```

### Cover a boundary, not the middle

One mid-month date proves little. Test the first and last day of a month, an
empty table, and a value that is exactly at the limit. Freeze the clock for the
date cases (see [Deterministic Time and Randomness](#deterministic-time-and-randomness)).

## API Testing Examples

### JSON Response Testing

```wren
// Parse JSON response
var data = Test.apiGet("/json").status(200).json()
Test.assert(data is List, "Expected JSON array")

// Insert test data
`INSERT OR REPLACE INTO counter (name, value) VALUES ('test1', 10)`.query()

// Complex validation with parsed JSON
var counters = Test.apiGet("/json").status(200).json()
Test.assert(counters.count >= 2, "Expected at least 2 counters")

// Iterate and validate specific values
for (counter in counters) {
  if (counter["name"] == "test1") {
    Test.assert(counter["value"] == "10", "Expected value '10'")
  }
}
```

### Mixed Assertion Styles

```wren
// Simple contains check
Test.get("/hi").status(200).contains("Hello World")

// Exact match
Test.get("/hi")
    .status(200)
    .equals("<h1>👋 Hello World</h1>")

// Multiple chained assertions
Test.get("/hi")
    .status(200)
    .contains("Hello")
    .contains("World")
    .notContains("Goodbye")
```

### Security and Negative Assertions

```wren
// Test what should NOT be present
Test.post("/password", {"password": "abc", "password-check": "xyz"})
    .status(200)
    .notContains("The passwords are the same")

Test.get("/hi")
    .status(200)
    .notContains("Error")
    .notContains("Failed")
```

### Form Testing

```wren
Test.post("/password", {"password": "123", "password-check": "123"})
    .status(200)
    .contains("The passwords are the same")

Test.post("/password", {"password": "123", "password-check": "321"})
    .status(200)
    .contains("The passwords are different")
```

## How It Works

1. **Module per file**: each test file compiles as its own Wren module with the
   runner prelude (`Test`, `Tests`, `Fixture`). Relative imports resolve from
   `_tests/`, and `_tests/_init.wren` runs in the same module, so its top-level
   variables are visible to the file.
2. **Request Building**: the `Test` class builds an HTTP request (method,
   headers, body).
3. **Internal Routing**: instead of opening a socket, the test runner invokes the
   real router and the Wren handler directly, in a fresh VM per request.
4. **Response Capture**: the response (status, headers, body) is captured and
   wrapped.
5. **Assertions**: each assertion checks the response and reports failures. The
   first failure aborts the file.
6. **Database**: one temporary database per run. Migrations run once, then each
   test file runs inside a savepoint that is rolled back when the file ends.

This approach is fast (no network overhead), reliable (no port conflicts), and
safe (a throwaway database, rolled back per file).

## Exit Codes

- `0`: All tests passed
- `1`: One or more tests failed

This makes it easy to integrate with CI/CD pipelines. `-q` drops the colors
and per-test lines down to a single summary line plus a `### FAIL` block per
failure -- friendlier for CI logs. It also silences the run's log: the
migrations it runs and the tests' `System.print`/`System.log` output go to a
discard stream. The summary and `### FAIL` blocks still print, because they go
straight to stdout. A migration that fails to compile or apply is replayed to
stderr and makes the run exit non-zero, so a deploy gate cannot pass silently.
Notices emitted while Bialet is still starting (an upload-cap adjustment, for
example) still appear, and with `-l FILE` the silenced records are not appended
to that file either:

```bash
$ bialet -T -q
1 of 1 tests failed in 20ms

### FAIL _tests/hi.wren:2 - hi
Expected code to be 200 but was 500
```

Without `-q`, migration and log output are printed as usual.

```yaml
# GitHub Actions example
- name: Run tests
  run: bialet -T -q
```

