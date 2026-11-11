# Common Pitfalls

These are the traps that bite hardest when building Bialet apps. Each one has
been reproduced against a running app; where an error message is quoted, it is
the text Bialet actually prints.

Most are Wren rules rather than Bialet rules, but they only become obvious once
a page that worked on an empty database meets its first row.

## 1. `toString` is a getter; `trim` is a method

```wren
name=(val) { _name = val.toString.trim() }    // correct
// name=(val) { _name = val.toString.trim }     // String does not implement 'trim'
```

`toString`, `count`, `keys` and `values` are getters: no parentheses. `trim` is
a **method**: it needs `()`. `toString()` is accepted as an alias for the getter
(the compiler treats the zero-argument call as the getter), so both spellings
work; `count()`, `keys()` and `values()` still fail. Forgetting the `()` on
`trim` is the usual source of 500s in setters.

## 2. Every method that produces a value needs an explicit `return`

Wren is not expression-oriented: a method returns the value of `return`, not the
last expression.

```wren
static forDay(day) {
  return `SELECT * FROM orders WHERE deliveryDate = ?`.fetch(day).to(Order)
}
```

A missing `return` yields `null`; the caller then does `for (row in null)`, which
raises a catchable `Value is not iterable.` error and turns the request into a
500. Checking the result first (`if (!rows) rows = []`) avoids the failed
request.

## 3. `BIALET_CONFIG` stores `(key, val)`, not `(key, value)`

```wren
Config.set("fee_amount", "20000")   // correct
// INSERT INTO BIALET_CONFIG (key, value) ...
// Query Error: table BIALET_CONFIG has no column named value
```

Use `Config.set/get/bool/num/enable/disable/json` (see
[Configuration](configuration.md)); never hand-write the table.

## 4. Query methods take at most three positional arguments

```wren
`SELECT * FROM x WHERE a=? AND b=? AND c=? AND d=?`.fetch([a, b, c, d])
// .fetch(a, b, c, d) -> Query does not implement 'fetch(_,_,_,_)'
```

The same limit applies to `.first`, `.val`, `.toNum`, `.toBool`, `.query` and
`.order`. Four or more values go in one List.

## 5. Every file imports what it uses

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

## 6. `_tests/_init.wren` variables are global to every test file

```wren
// _tests/_init.wren
var day = "2026-05-04"

// _tests/flujo.wren
// var day = "2026-05-04"   // Module variable is already defined: day
```

Use the variable from `_init.wren`, give the local one another name, or wrap the
shared data in a function. `Test` is also preloaded: never
`import "bialet/test" for Test`.

## 7. Tests use a temporary database; manual HTTP does not

`bialet -T` runs against a temporary database, so test rows do not pollute
`_db.sqlite3`. Manual `curl` POSTs against a running server **do** write to
`_db.sqlite3`: when you verify by hand, delete the rows you inserted (or back up
and restore the file).

## 8. `Date.new(unixSeconds)` does not exist; use `Date.fromUnix`

`Date.new(1792292400)` is a year-1792292400 date, not a timestamp. Build a date
from epoch seconds with `Date.fromUnix(seconds)` (formatted in `Date.tz`), and
go the other way with `.unix`. For "now" use `Date.new()`.

## 9. `Num.fromString` does not understand `1.234,50`

It returns null for thousands separators and decimal commas. Normalize first
(see `Money.parse` in [Recipes](recipes.md)), or store integer cents and parse
the form value with the helper.

## 10. Do not nest the same outermost HTML tag

Bialet's inline-HTML parser rejects an outermost tag that also appears as a
direct child at any level:

```wren
var bad = <section><section>Nested</section></section>
// Cannot nest <section> inside <section>
```

The error points at the closing tag, not the opening one, which makes it hard to
find. Wrap with a different tag (`<div><section>...`) or split the view into
partials. See [Template](template.md) for the nesting rules.

## 11. Assigning a property needs an explicit setter

```wren
class Wine {
  id { _id }
  id=(value) { _id = value }   // without this line `wine.id = x` fails
}
```

Wren does not synthesize setters. `Wine does not implement 'id=(_)'` means the
getter exists but the setter does not. The constructor writing `_id` is not a
public setter.

## 12. Do not put a Query inside `%(...)` or `{{ }}`

```wren
// wrong: Query is not allowed inside handlebars or interpolated expressions
System.log("rows: %(`SELECT COUNT(*) FROM loans`.toNum)")

// correct
var count = `SELECT COUNT(*) FROM loans`.toNum
System.log("rows: %(count)")
```

This bites hardest in `_tests/_init.wren`, where the whole suite then fails
before the app is exercised.

## 13. Zero-argument methods still need `()`

```wren
class Klass {
  static total() { 42 }
}
Klass.total()   // correct
// Klass.total  -> the method object, not the number
```

Getters (`count`, `toString`, `keys`) are defined without parentheses and
called without them. If you wrote `total() { ... }`, call it with `()`.

## 14. `_init.wren` runs once; do not collide with UNIQUE keys

A test that inserts the same unique value as `_tests/_init.wren` fails before it
exercises anything. Use `INSERT OR IGNORE`, or seed distinct values per file.

## 15. HTML templates use `{{ }}`, not `%(...)`

```wren
// correct in a route/template HTML block
<p>Total: $ {{ Money.fmt(total) }}</p>

// wrong: %(...) is Wren string interpolation, and HTML renders it literally
<p>Total: $ %(Money.fmt(total))</p>
```

`%(...)` only works inside a Wren string literal. In an HTML block Bialet uses
`{{ expression }}`. See [Template](template.md).

## 16. Name the columns in every INSERT

```wren
// breaks as soon as the table has an id/createdAt column:
`INSERT INTO expenses VALUES (?, ?, ?)`
// correct:
`INSERT INTO expenses (concept, cents, spentOn) VALUES (?, ?, ?)`
```

`expenses has 7 columns but 6 values supplied` is this mistake. Naming the
columns also keeps the insert valid when a later migration adds a column.

## 17. The form `name` must match `Request.post` exactly

```wren
<input name="dia" type="date">
var day = Request.post("day")   // null: the key is "dia"
```

A mismatch silently yields null; the POST returns 302 but nothing is saved, or
the handler crashes formatting null. Keep the HTML `name` and the
`Request.post`/`Request.get` key identical (or use the same constant).

## 18. Strings do not implement `<` or `>`

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

## 19. `{{ }}` must sit inside an HTML tag

At the top level of a template, `{{ ... }}` is parsed as a Wren map literal and
fails with `Expect '}' after map entries`:

```wren
// wrong: no enclosing tag
return {{ count > 0 && <div class="box">...</div> }}

// correct: interpolate inside an element
return <main>{{ count > 0 && <div class="box">...</div> }}</main>
```

Once the expression is inside a tag, an HTML block works as an operand. If it
gets long, assign it to a variable above the template and interpolate that
inside a tag.

## 20. `""` is truthy in Wren

```wren
var name = Request.post("name") || ""
if (!name) { ... }          // never true for ""
if (name.trim().count == 0) { ... }   // correct empty check
```

Only `null` and `false` are falsy. Validate empty text with `trim().count == 0`
(or `trim() == ""`), not `!text`.

## 21. Enforce rules in the domain, not only in the UI

Disabling a button is not validation: a direct `POST` bypasses it. Put the rule
in the domain method (and a UNIQUE index where the database can express it).
Example: "one open loan per book" must be checked in `Loan.create`, not only by
rendering the button as disabled.

