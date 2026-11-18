# Recipes

Short, runnable answers to the problems that come up in most apps: dates,
money, CSV exports, and a few API helpers. Every snippet here was executed with
`bialet -r`, or validated as an app file with `bialet -t`.

Read [Common Pitfalls](pitfalls.md) as well: it covers the API traps these
recipes work around (`toString` is a getter, a missing `return` returns null,
iterating null crashes, a literal `%` is `\x25`, a Query takes at most three
arguments).

## Dates: add days, add months, week start, due dates

`Date` shifts instants by seconds with `+`/`-`, subtracts to seconds, and moves
calendar days with `addDays`. Month arithmetic is the one thing it does not
have, so keep a small clamping helper for it:

```wren
class Dates {
  static leap(y) { (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0) }

  static daysInMonth(y, m) {
    if (m == 2) return Dates.leap(y) ? 29 : 28
    return [4, 6, 9, 11].contains(m) ? 30 : 31
  }

  // Adds calendar months, clamping the day: 2024-01-31 + 1 -> 2024-02-29.
  static addMonths(date, n) {
    var total = date.year * 12 + (date.month - 1) + n
    var y = (total / 12).floor
    var m = total % 12 + 1
    var d = date.day
    var last = Dates.daysInMonth(y, m)
    if (d > last) d = last
    return Date.new(y, m, d, date.hours, date.minutes, date.seconds, date.tz)
  }

  // 0 = Sunday, so shift by six to make Monday the first day.
  static startOfWeek(date) { date.addDays(-((date.dayOfWeek + 6) % 7)) }
}
```

```wren
Date.fromUnix(1700000000)                    // 2023-11-14 22:13:20 in Date.tz
Date.new(1700000000, "UTC")                  // the same instant, in UTC
Date.new(2024, 1, 31).addDays(1)             // 2024-02-01 00:00:00
Date.new(2024, 2, 28).addDays(1)             // 2024-02-29 00:00:00
Date.new(2024, 9, 13) + 86400                // 2024-09-14: + one day of seconds
Date.new(2024, 9, 14) - Date.new(2024, 9, 13) // 86400
Dates.addMonths(Date.new(2024, 1, 31), 1).format("#Y-#m-#d") // 2024-02-29
Dates.startOfWeek(Date.new(2026, 10, 18)).format("#Y-#m-#d") // 2026-10-12
```

`addDays` keeps the wall clock and zone, so it is safe across leap days, month
ends and DST changes; `+`/`-` with a number is an exact number of seconds. For a
due date, add the days the contract means rather than 30 × 86400 seconds.

Format tokens are strftime-style with `#` instead of `%`: `#Y #m #d #H #M #S`,
`#w` (0 = Sunday), `#u` (1 = Monday), `#V` (ISO week), `#F` (ISO date). The
full table is in [Date and Time Functions](datetime.md).

Comparing dates is safe with `<`, `>`, `==`. Comparing date **strings** is not:
`"2026-01-01" < "2026-02-01"` raises `String does not implement '<(_)'`. Parse
to `Date.new(text)` first, and guard the null that `.first`/`.val` return when
there is no row.

## Money: store cents, print two decimals

Wren has no `Num.format`. Store amounts as integer cents and format them with a
helper; never do float arithmetic on pesos/dollars across many rows.

```wren
class Money {
  // Accepts "1234.5", "1.234,50", "12,5", "19.99" and null.
  static parse(text) {
    if (text == null) return 0
    var t = "%(text)".trim().replace(" ", "")
    if (t == "") return 0
    if (t.contains(",")) {
      // Spanish format: dots are thousands, comma is the decimal separator.
      t = t.replace(".", "").replace(",", ".")
    } else if (t.contains(".")) {
      var parts = t.split(".")
      var last = parts[parts.count - 1]
      // "1.234" is thousands; "19.99" is decimal.
      if (parts.count == 2 && last.count == 3) t = t.replace(".", "")
    }
    return Num.fromString(t) || 0
  }

  static cents(text) { (Money.parse(text) * 100).round }

  static fmt(cents) {
    var negative = cents < 0
    var abs = negative ? -cents : cents
    var whole = (abs / 100).floor
    var frac = abs - whole * 100
    return "%(negative ? "-" : "")%(whole).%(frac < 10 ? "0" : "")%(frac)"
  }
}
```

```wren
Money.fmt(Money.cents("1234.567")) // 1234.57
Money.fmt(Money.cents("1.234,50")) // 1234.50
Money.fmt(Money.cents("12,5"))     // 12.50
Money.cents("19.99")               // 1999 (store this INTEGER)
Money.cents(null)                  // 0
```

In SQL, store `INTEGER` cents and sum with `SUM(cents)`; convert only when
rendering. Keep the arithmetic in integers: summing `REAL` amounts across
hundreds of rows accumulates rounding error.

## CSV download

`Response.header(name, value)` sets the response header; there is no
`Response.csv`/`download` helper. Build the body, set both headers, and
`return` it:

```wren
var rows = `SELECT name, amount FROM expenses ORDER BY id`.fetch

var lines = ["concepto,importe"]
for (row in rows) {
  var name = "%(row["name"])".replace("\"", "\"\"")
  // Quote every field, including numbers: "12.500,50" contains a comma and
  // would shift the columns if it were left bare.
  var amount = "%(row["amount"])".replace("\"", "\"\"")
  lines.add("\"%(name)\",\"%(amount)\"")
}

Response.header("Content-Type", "text/csv; charset=utf-8")
Response.header("Content-Disposition", "attachment; filename=\"gastos.csv\"")
return lines.join("\n") + "\n"
```

Quote a field whenever it contains a comma, a quote or a newline; double every
inner quote. Link to the route with a normal `<a href="/exportar">Exportar CSV</a>`.
In tests, assert the header instead of the whole file:
`Test.get("/exportar").status(200).header("Content-Type", "text/csv; charset=utf-8")`.

## `bialet -r` is not a request

`bialet -r` executes a bare Wren script. It does **not** create the `BIALET_*`
tables or a request/session, so these fail there:

- `Session.new()` → `Query Error no such table: BIALET_SESSION`
- `Config.get(...)` → `Query Error no such table: BIALET_CONFIG`
- App queries (`SELECT ... FROM my_table`) → `no such table`

Use it only for pure language questions (`Date`, `Num`, `String`, `List`,
`Map`). For framework behaviour, write a test in `_tests/` and run `bialet -T`,
or start the server and try the page.

## Util and Config

Stable `Util` helpers:

| Call | Returns |
| --- | --- |
| `Util.randomString(n)` | random URL-safe string of length `n` (tokens, ids) |
| `Util.hash(password)` | salted PBKDF2 hash (store this in the DB) |
| `Util.verify(password, hash)` | `true` when the password matches the hash |
| `Util.toNum(value)` | `0` for null/garbage, otherwise the number |
| `Util.htmlEscape(text)` | escaped HTML text |
| `Util.isDigits(text)` | `true` when every character is `0`–`9` |
| `Util.secureEquals(a, b)` | constant-time string comparison |
| `Util.encrypt(value, secret)` | AES-256-GCM envelope (text, storable) |
| `Util.decrypt(envelope, secret)` | plaintext, or `null` if it does not authenticate |

`Config` is backed by `BIALET_CONFIG`:

```wren
Config.get("title")            // String or null
Config.set("title", "Mi app")
Config.bool("beta_features")   // true only for "1"/"true"
Config.num("page_size")        // Num
Config.enable("beta_features") // same as Config.set("beta_features", "1")
Config.disable("maintenance")  // same as Config.set("maintenance", "0")
Config.json("theme")           // parsed JSON value
Config.encrypt("smtp_password", "...")  // AES-256-GCM under --secret-key-file
Config.decrypt("smtp_password")         // plaintext, or null
```

`Config.bool(key)` is an allow-list: `true` for `"1"` and `"true"`
(case-insensitive), `false` for everything else **including a missing key**. For
an opt-in flag (a test mode, a feature gate) that is the behavior you want, but
write `Config.get(key) == "1"` when the default should be obvious at the call
site. See [Configuration](configuration.md).

## Queries with more than three parameters

A Query method takes at most three positional parameters. With four or more it
raises `Query does not implement 'fetch(_,_,_,_)'` (or `fetch(_,_,_,_,_)` with
five). Pass one array instead:

```wren
var rows = `SELECT * FROM items WHERE a = ? AND b = ? AND c = ? AND d = ?`
  .fetch(["1", "2", "3", "4"])
```

The same rule applies to `.first`, `.val`, `.toNum`, `.query`, `.toBool` and
`.order`: three arguments maximum, then an array. See [Database](database.md).

## Shared POST actions: one route, a hidden `action`

A list page usually has several one-button POST operations — toggle, delete,
rename — that all return to the same place. Post them to one route and dispatch
on a hidden `action` field instead of copying the CSRF check into a file per
operation. The form helper lives in `_app/`, so every template renders the
token, the row id and the return path the same way.

```wren
// _app/actions.wren
class Actions {
  // The only redirect targets the app accepts: an absolute local path.
  // "//evil.example" also starts with "/" but names another host, and a value
  // without the leading slash is not a path at all.
  static safeBack(value) {
    var path = "%(value)".trim()
    if (!path.startsWith("/") || path.startsWith("//")) return "/"
    return path
  }

  static form(action, id, back, label) {
    return <form method="post" action="/actions">
      {{ Session.csrf }}
      <input type="hidden" name="action" value="{{ action }}" />
      <input type="hidden" name="id" value="{{ id }}" />
      <input type="hidden" name="back" value="{{ back }}" />
      <button>{{ label }}</button>
    </form>
  }
}
```

```wren
// actions.wren — every one-button POST lands here
import "_app/actions" for Actions

if (!Request.isPost) return Response.redirect("/")
if (!Session.csrfOk) {
  Response.status(403)
  return "Invalid CSRF token"
}

var action = Request.post("action", "")
var id = Request.post("id", "")
var back = Actions.safeBack(Request.post("back", "/"))

if (action == "toggle") `UPDATE tasks SET done = 1 - done WHERE id = ?`.query(id)
if (action == "delete") `DELETE FROM tasks WHERE id = ?`.query(id)
return Response.redirect(back)
```

```wren
// index.wren — one form per row operation
return <ul>
  {{ tasks.map { |task| <li>
    {{ Actions.form("toggle", task.id, Request.uri, task.done ? "Undo" : "Done") }}
    {{ Actions.form("delete", task.id, Request.uri, "Delete") }}
  </li> } }}
</ul>
```

- The hidden `action` is an ordinary form field. `Request.post("action", "")`
  returns `""` when it is missing, so an unknown or empty value falls through
  the `if`s instead of crashing on null.
- `safeBack` refuses `//host` as well as anything that is not a local path, so
  a crafted `back` cannot redirect a logged-in user to another site.
- One route means one CSRF check and one redirect policy. A new action is a new
  `if`, not a new file with a copy of the same guard.
- Do not nest forms: a form inside another form is invalid HTML, so render one
  form per button.

## Multi-step form state in the query string

A wizard needs neither session state nor JavaScript. Put the answers given so
far in the URL: each step is a `GET` whose hidden fields carry the earlier
answers and whose `step` field names the next screen. Reload, Back and a shared
link all keep working, and there is no server-side draft to expire.

```wren
// survey.wren — /survey?step=plan&name=Ana
class Survey {
  static questions { ["name", "plan", "confirm"] }

  // The step after [step]; unknown input starts over at the first question.
  static nextQuestion(step) {
    for (i in 0...Survey.questions.count) {
      if (Survey.questions[i] == step && i + 1 < Survey.questions.count) {
        return Survey.questions[i + 1]
      }
    }
    return Survey.questions[0]
  }
}

var step = Request.get("step", "name")
var name = Request.get("name", "")
var plan = Request.get("plan", "")

if (step == "confirm") {
  return <main>
    <h1>Confirm</h1>
    <p>Name: {{ name }}</p>
    <p>Plan: {{ plan }}</p>
    <form method="post" action="/survey">
      {{ Session.csrf }}
      <input type="hidden" name="name" value="{{ name }}" />
      <input type="hidden" name="plan" value="{{ plan }}" />
      <button>Send</button>
    </form>
  </main>
}

if (step == "plan") {
  return <main>
    <h1>Which plan?</h1>
    <form method="get" action="/survey">
      <input type="hidden" name="step" value="{{ Survey.nextQuestion(step) }}" />
      <input type="hidden" name="name" value="{{ name }}" />
      <button name="plan" value="basic">Basic</button>
      <button name="plan" value="pro">Pro</button>
    </form>
  </main>
}

return <main>
  <h1>Your name?</h1>
  <form method="get" action="/survey">
    <input type="hidden" name="step" value="{{ Survey.nextQuestion(step) }}" />
    <input name="name" value="{{ name }}" />
    <button>Continue</button>
  </form>
</main>
```

- Every step is a `GET`, so Back, reload and a pasted URL land on a working
  page. Nothing is stored server-side, so nothing leaks between visitors.
- `Request.get(key, "")` gives a missing answer a safe empty value without
  hiding a real empty answer: `""` is a value the user submitted, `null` is an
  absent parameter.
- `<button name="plan" value="pro">` puts the choice in the query string on
  click: both buttons submit the same form and only `plan` differs. No
  JavaScript and no per-step route.
- The final step is an ordinary `POST` with `Session.csrf`; only the answers
  travel in the URL, while the write itself stays protected.

## Tests: one database, no noise

- Every file in `_tests/` runs in the same process against **one** temporary
  database for that run. A row inserted by `a.wren` is visible to `b.wren`;
  do not assume each file starts clean.
- Seed shared rows in `_tests/_init.wren`, which runs once before the files.
- `System.print` output becomes part of the test log. Never dump a whole
  table or a list of sessions; print one scalar when you need to debug, and
  delete the debug test afterwards. Hundreds of `debugN.wren` files make every
  later run slower and bury the real failure.
- `Test` has no cookie jar. For session-scoped behaviour, test the domain
  class directly, or assert the redirect and the row it wrote. When the app
  accepts an explicit header, use
  `.setHeader("Cookie", "session=...")` before the assertion.
- Never `import "bialet/test" for Test`: the runner preloads `Test`, and the
  import fails with `Module variable is already defined`.
- Assertions are lazy: the request fires on the first `.status`/`.contains`.
  Chain them on the same `Test.get(...)` call.
- `.contains` matches the raw HTML. Interpolation HTML-escapes strings, so a
  title with accents/quotes may appear as entities; assert on text that is
  stable, or read the stored row instead.

See [Testing in Bialet](tests.md) for the full runner reference.
