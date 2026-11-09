# Common Pitfalls

These are the traps that bite hardest when building Bialet apps. Each one has
been reproduced against a running app; where an error message is quoted, it is
the text Bialet actually prints.

Most are Wren rules rather than Bialet rules, but they only become obvious once
a page that worked on an empty database meets its first row.

## 1. `toString` is a getter; `trim` is a method

```wren
name=(val) { _name = val.toString.trim() }    // correct
// name=(val) { _name = val.toString().trim() } // String does not implement 'toString()'
// name=(val) { _name = val.toString.trim }     // String does not implement 'trim'
```

`toString`, `count`, `keys` and `values` are getters: no parentheses. `trim` is
a **method**: it needs `()`. Mixing the two forms is the most common source of
500s in setters.

## 2. Never compare a Date (or a user class) to `null` with `date == null`

```wren
if (!date) { ... }          // safe
if (null == date) { ... }   // safe
// if (date == null) ...    // Runtime Error: Null does not implement 'unix'.
```

`date == null` calls `Date.==`, which reads `other.unix`; `null` has no `unix`.
`!date` is `true` only when the value is null (a Date makes `!date` return null,
which is falsy), so it is the idiomatic guard. The same rule applies to your own
classes: put the null check on the left, or test `if (!value)`.

## 3. Every method that produces a value needs an explicit `return`

Wren is not expression-oriented: a method returns the value of `return`, not the
last expression.

```wren
static forDay(day) {
  return `SELECT * FROM orders WHERE deliveryDate = ?`.fetch(day).to(Order)
}
```

A missing `return` yields `null`; the caller then does `for (row in null)` and
**the whole Bialet process crashes with SIGSEGV (exit 139)**. The browser sees
`000` / an empty reply, not a friendly error.

## 4. Never iterate a value that may be null

```wren
var rows = `SELECT ...`.fetch
if (!rows) rows = []
for (row in rows) { ... }
```

`for (x in null)` is a hard crash, not an exception. Guard every Query result
and every helper that can return null. The same applies to a Map row:
`row.values` / `row.keys` on a null row crash; check `if (!row)` first.

## 5. A literal `%` in a Wren string is `\x25`

```wren
System.print("100\x25")   // 100%
// System.print("100%")   // Compilation Error: Expect '(' after '%'.
```

`%(` starts interpolation, so a bare `%` is a syntax error. In an **HTML
template** `%` is plain text: write `100%` there (if the template parser
complains, use the entity `&#37;`). Do not write `\x25` in HTML: it renders
literally as `\x25`.

## 6. `BIALET_CONFIG` stores `(key, val)`, not `(key, value)`

```wren
Config.set("fee_amount", "20000")   // correct
// INSERT INTO BIALET_CONFIG (key, value) ...
// Query Error: table BIALET_CONFIG has no column named value
```

Use `Config.set/get/bool/num/enable/disable/json` (see
[Configuration](configuration.md)); never hand-write the table.

## 7. Query methods take at most three positional arguments

```wren
`SELECT * FROM x WHERE a=? AND b=? AND c=? AND d=?`.fetch([a, b, c, d])
// .fetch(a, b, c, d) -> Query does not implement 'fetch(_,_,_,_)'
```

The same limit applies to `.first`, `.val`, `.toNum`, `.toBool`, `.query` and
`.order`. Four or more values go in one List.

## 8. Every file imports what it uses

```wren
// route file
import "/_app/template" for Layout
import "/_app/domain" for Item, Member

// _app/template.wren that calls Periods
import "/_app/domain" for Periods
```

A template is a separate module: it does not inherit the route's imports. This
is why `{{ Periods.label(...) }}` can fail with `Variable is used but not
defined` even though `Periods` exists in `_app/domain.wren`.

## 9. `_tests/_init.wren` variables are global to every test file

```wren
// _tests/_init.wren
var day = "2026-05-04"

// _tests/flujo.wren
// var day = "2026-05-04"   // Module variable is already defined: day
```

Use the variable from `_init.wren`, give the local one another name, or wrap the
shared data in a function. `Test` is also preloaded: never
`import "bialet/test" for Test`.

## 10. Tests use a temporary database; manual HTTP does not

`bialet -T` runs against a temporary database, so test rows do not pollute
`_db.sqlite3`. Manual `curl` POSTs against a running server **do** write to
`_db.sqlite3`: when you verify by hand, delete the rows you inserted (or back up
and restore the file).

## 11. `Date.new(unixSeconds)` does not exist

`Date.new(1792292400)` is a year-1792292400 date, not a timestamp. For date
arithmetic use the helpers in [Recipes](recipes.md); for "now" use `Date.new()`.

## 12. `Num.fromString` does not understand `1.234,50`

It returns null for thousands separators and decimal commas. Normalize first
(see `Money.parse` in [Recipes](recipes.md)), or store integer cents and parse
the form value with the helper.

## 13. Assertions match raw markup

`Test.get("/").contains("device")` also matches `width=device-width` in the
viewport meta tag. Assert on visible, unique text (a heading, a row label), not
on a substring that appears in the HTML boilerplate.

## 14. Do not nest the same outermost HTML tag

Bialet's inline-HTML parser rejects an outermost tag that also appears as a
direct child at any level:

```wren
var bad = <section><section>Nested</section></section>
// Cannot nest <section> inside <section>
```

The error points at the closing tag, not the opening one, which makes it hard to
find. Wrap with a different tag (`<div><section>...`) or split the view into
partials. See [Template](template.md) for the nesting rules.

## 15. Assigning a property needs an explicit setter

```wren
class Wine {
  id { _id }
  id=(value) { _id = value }   // without this line `wine.id = x` fails
}
```

Wren does not synthesize setters. `Wine does not implement 'id=(_)'` means the
getter exists but the setter does not. The constructor writing `_id` is not a
public setter.

## 16. Do not put a Query inside `%(...)` or `{{ }}`

```wren
// wrong: Query is not allowed inside handlebars or interpolated expressions
System.log("rows: %(`SELECT COUNT(*) FROM loans`.toNum)")

// correct
var count = `SELECT COUNT(*) FROM loans`.toNum
System.log("rows: %(count)")
```

This bites hardest in `_tests/_init.wren`, where the whole suite then fails
before the app is exercised.

## 17. Aggregates return null on an empty table

`SELECT MAX(id)`, `SELECT SUM(x)` and friends return null when there are no
rows. `null + 1` crashes. Use a default:

```wren
var max = `SELECT COALESCE(MAX(CAST(number AS INTEGER)), 0) FROM members`.toNum
```

This is the classic 500 on a brand-new database.

## 18. `List` has no `reverse`

`[1,2,3].reverse` raises `List does not implement 'reverse'`. Build a new list:

```wren
var out = []
for (i in 0...xs.count) out.add(xs[xs.count - 1 - i])
```

## 19. CSRF is on the `session` instance, and tests have no cookie jar

```wren
var session = Session.new()
if (Request.isPost) {
  if (!session.csrfOk && Config.get("test_mode") != "1") return Response.status(403)
  ...
}
{{ session.csrf }}
```

`Session.csrf` / `Session.csrfOk` are **not** static: they are members of
`session = Session.new()`. In tests, `Test.post` cannot reuse the token from a
previous `Test.get` (there is no cookie jar), so a CSRF-protected form returns
200 instead of 302. The same applies to any session value: a name set by one
`Test.get` is not visible to the next `Test.post`. Pass the value in the body
(or test the domain method directly) instead of relying on session state. Make
the check test-aware with a config flag and set it once in `_tests/_init.wren`:

```wren
// _tests/_init.wren
Config.set("test_mode", "1")
```

Read that flag with `Config.get("test_mode") == "1"`: it is explicit about the
"off unless set" default. (`Config.bool` works too — it returns `true` only for
`"1"`/`"true"`, and `false` for a missing key — but `get` makes the intent
obvious at the call site.) Never disable CSRF outside the test flag.

Tests also share one `BIALET_CONFIG`: a test that sets a config value leaks it
into every other test file. Reset the key or use a unique one.

## 20. Zero-argument methods still need `()`

```wren
class Klass {
  static total() { 42 }
}
Klass.total()   // correct
// Klass.total  -> the method object, not the number
```

Getters (`count`, `toString`, `keys`) are defined without parentheses and
called without them. If you wrote `total() { ... }`, call it with `()`.

## 21. `Map` has no `get(key)`

```wren
var m = {"a": 1}
m["a"]              // 1
m["missing"]        // null
// m.get("a")       // Map does not implement 'get(_)'
m.containsKey("a")  // true
```

## 22. `_init.wren` runs once; do not collide with UNIQUE keys

A test that inserts the same unique value as `_tests/_init.wren` fails before it
exercises anything. Use `INSERT OR IGNORE`, or seed distinct values per file.

## 23. HTML templates use `{{ }}`, not `%(...)`

```wren
// correct in a route/template HTML block
<p>Total: $ {{ Money.fmt(total) }}</p>

// wrong: %(...) is Wren string interpolation, and HTML renders it literally
<p>Total: $ %(Money.fmt(total))</p>
```

`%(...)` only works inside a Wren string literal. In an HTML block Bialet uses
`{{ expression }}`. See [Template](template.md).

## 24. Name the columns in every INSERT

```wren
// breaks as soon as the table has an id/createdAt column:
`INSERT INTO expenses VALUES (?, ?, ?)`
// correct:
`INSERT INTO expenses (concept, cents, spentOn) VALUES (?, ?, ?)`
```

`expenses has 7 columns but 6 values supplied` is this mistake. Naming the
columns also keeps the insert valid when a later migration adds a column.

## 25. The form `name` must match `Request.post` exactly

```wren
<input name="dia" type="date">
var day = Request.post("day")   // null: the key is "dia"
```

A mismatch silently yields null; the POST returns 302 but nothing is saved, or
the handler crashes formatting null. Keep the HTML `name` and the
`Request.post`/`Request.get` key identical (or use the same constant).

## 26. Strings do not implement `<` or `>`

```wren
"2026-01-01" < "2026-02-01"
// Runtime Error: String does not implement '<(_)'.
```

Strings only support `==` / `!=`. Compare parsed values instead:

```wren
Date.new("2026-01-01") < Date.new("2026-02-01")
Num.fromString(a) < Num.fromString(b)
```

This is the classic "the list works until the first row exists" 500.

## 27. A failed migration can still be marked as applied

If the SQL in `Db.migrate` fails (`incomplete input`, a missing column, ...),
Bialet can still record the version in `BIALET_MIGRATIONS`. Fixing the `.wren`
then does nothing because the migration is considered done. Either bump the
version string (`"Create coverage table v2"`) or delete the row:

```sql
DELETE FROM BIALET_MIGRATIONS WHERE version = 'Create coverage table';
```

Check this before assuming a schema change took effect.

## 28. Do not call `Date.new()` inside a `Db.migrate` callback

`Db.migrate` callbacks run before `Date.init`, so any date construction or date
arithmetic there crashes the whole process with SIGSEGV (exit 139) and the app
never boots:

```wren
// wrong: Date is not initialized yet
Db.migrate("Seed", Fn.new {
  var today = Date.new()
  ...
})

// correct: let SQL produce the date
Db.migrate("Seed", `
  INSERT INTO entries (day) VALUES (date('now'))
`)
```

Seed dates with SQL (`date('now')`, `strftime`) or lazily from a route/domain
method after the app is up.

## 29. `Num.fromString(null)` raises "Argument must be a string"

```wren
// wrong: a missing form field is null
_amount = Num.fromString(data["amount"])

// correct: stringify with a default, then fall back
_amount = Num.fromString("%(data["amount"] || 0)") || 0
```

`Num.fromString` only accepts a String. A null (or an absent key) crashes the
constructor, so every POST 500s even though the GETs work.

## 30. Do not build an HTML block inside `{{ }}`

```wren
// wrong: parsed as a map/block -> "Expect ':' after map key"
{{ count > 0 && <div class="box">...</div> }}

// correct: build the string first
var extra = count > 0 ? <div class="box">...</div> : ""
{{ extra }}
```

Bialet's `{{ }}` expects an expression, not an inline HTML tree. Assign the
block to a variable (or a small helper) and interpolate that.

## 31. Private apps need a first-run credential

An empty admin key/password either locks everyone out or opens everything. Seed
one in the migration (`INSERT OR IGNORE ...`) or add a first-run page that
creates the first user and shows the recovery key. Then verify login end to end;
a redirect loop on the login page usually means there is no user to log in with.

## 32. `""` is truthy in Wren

```wren
var name = Request.post("name") || ""
if (!name) { ... }          // never true for ""
if (name.trim().count == 0) { ... }   // correct empty check
```

Only `null` and `false` are falsy. Validate empty text with `trim().count == 0`
(or `trim() == ""`), not `!text`.

## 33. HTML whitespace makes multi-word `.contains` assertions fragile

```wren
Test.get("/").contains("1 paciente para")  // fails if the template wraps the line
```

HTML collapses or inserts whitespace between words. Assert a single stable
token (a heading, a name, a badge) instead of a multi-word sentence.

## 34. Enforce rules in the domain, not only in the UI

Disabling a button is not validation: a direct `POST` bypasses it. Put the rule
in the domain method (and a UNIQUE index where the database can express it).
Example: "one open loan per book" must be checked in `Loan.create`, not only by
rendering the button as disabled.

## 35. `bialet -r` is not a request

`bialet -r` executes a bare Wren script. It does **not** create the `BIALET_*`
tables or a request/session, so these fail there:

- `Session.new()` → `Query Error no such table: BIALET_SESSION`
- `Config.get(...)` → `Query Error no such table: BIALET_CONFIG`
- App queries (`SELECT ... FROM my_table`) → `no such table`

Use it only for pure language questions (`Date`, `Num`, `String`, `List`,
`Map`). For framework behaviour, write a test in `_tests/` and run `bialet -T`,
or start the server and try the page.
