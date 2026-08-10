# Carlos's Session Notes

47. 20 years of vanilla PHP + MySQL. I build the same way in every language:
`if ($_POST)` controllers, files-per-URL, explicit SQL, `htmlspecialchars()`
on everything, and a migration file I can read. I don't trust magic, and when
the docs get vague I read the source. That's what this session was: does this
little C+Wren thing do my sessions, CSRF, escaping, and SQL for me, or am I
back in 2008?

Short answer: escaping and SQL injection are done FOR me, properly. Sessions
and CSRF are done for me too, and the multi-form token case — the thing that
used to be broken — actually works now. But there is one silent Wren footgun
that ate an hour of my life, and the docs don't warn you about it anywhere.

---

## What I set up

A classic MVC todo app, exactly the shape I'd build in PHP:

```
todo/
├── _app/
│   ├── migration.wren      # Db.migrate("Create tasks table", `CREATE TABLE ...`)
│   ├── domain.wren         # Task model: list/find/create/toggle/delete, explicit SQL
│   └── template.wren       # Template.layout() shared HTML
├── index.wren              # list + add form, controller on top, view below
├── toggle.wren             # POST-only, redirect back
├── delete.wren             # POST-only, redirect back
├── style.css
└── _db.sqlite3             # created automatically on first run
```

Ran it with `nohup .../build/bialet -p 7012 ./todo &`. First run created the
SQLite DB and applied the migration without me asking:

```
2026-08-10 17:00:48 Log Migration applied - Create tasks table
```

No config, no build step, no npm. That part is genuinely nice. One binary and
it just runs.

## What worked first try

- **PRG.** `if (Request.isPost) { ...; return Response.redirect("/") }`.
  Every POST returned `HTTP/1.1 302` + `Location: /`. Refresh won't resubmit.
- **Parameterized SQL.** Backtick queries with `?` placeholders. The compiler
  refuses string interpolation into a backtick, so the injection path is
  closed by construction. I verified with `'; DROP TABLE tasks; --` as a task
  title: it was stored **literally** as text and rendered escaped. No drop.
- **`Request.post` returns `null`** when the field is missing. The docs scream
  this on every page and they're right to. `(Request.post("title") || "").trim()`
  everywhere. Without the guard it crashes the request.
- **DB values come back as strings.** `done` is an INTEGER in SQLite but I
  compare `row["done"] == "1"`. Annoying, documented, handled.
- **`.to(Task)` mapping.** `first(id).to(Task)` returns `null` for a missing
  row because `Null.to` is a no-op in the core lib. So `find()` is safe
  without a null dance. That surprised me — in a good way.
- **Live reload.** Editing `template.wren` hot-reloaded without a restart.
- **Private files.** `/_app`, `/_db.sqlite3`, `/_app/domain.wren` all return
  **403**. Unknown paths return 404.

## What broke, and the exact errors

### 1. `Template.new()` — no constructor, no dice

First boot gave me:

```
Runtime Error Template metaclass does not implement 'new()'.
Stack Error .../todo/index.wren line 30 (script)
```

Wren has **no implicit constructor**. `Template.new()` only exists if you
declare `construct new() {}`. In PHP every object has a default constructor,
so I wrote `class Template { layout(content) { ... } }` and called
`Template.new().layout(...)` exactly like the docs show — and it blew up. The
docs' example *happens* to include `construct new()`, but nowhere does it say
"this is mandatory". Silent until runtime.

Fix: `construct new() {}` in the class.

### 2. The big one: my model silently returned 0 rows

After the add forms all returned 302 and sqlite confirmed **4 rows in the
table**, the page rendered the empty state:

```sql
sqlite> SELECT id, title, done FROM tasks;
1|buy milk|0
2|write report|0
3|<script>alert(1)</script>|0
4|'; DROP TABLE tasks; --|0
```

Page: "Nothing here yet. Add a task above." No error anywhere. Server log:
nothing. My `list()` was:

```wren
static list() {
  `SELECT * FROM tasks ORDER BY id`.fetch.to(Task)
}
```

That's the documented pattern. `fetch.to(Task)` returns 4 rows when I do it
inline, but the *same expression* inside a method body returned nothing. I
proved it side by side:

```
DEBUG Task.list count=0      // the method
DEBUG inline chain count=4   // identical expression at top level
```

I read the compiler source to find out why. `wren_compiler.c`, `finishBlock`:

```c
// If there's no line after the "{", it's a single-expression body.
if(!matchLine(compiler)) {
  expression(compiler);
  ...
  return true;          // <- implicit return ONLY here
}
...
return false;           // statement body -> implicit return null
```

**A method body is an implicit-return "expression body" only when the
expression starts on the same line as the `{`.** Put a newline after `{` and
it becomes a *statement body* that returns `null`. No warning. My `{\n  expr\n}`
form was a statement body. The docs' model example works only because it puts
the opening backtick on the `{` line by accident.

This is stock Wren behavior, not a Bialet regression — but the docs
("A Wren block that contains a single expression implicitly returns that
expression") never warn that the newline kills it. For a PHP dev this is a
trap: silent empty results, no error message, "it worked when I typed a
number" energy.

Fix: **explicit `return` in every method body.** Carlos doesn't trust implicit
returns anymore. There's a comment in `domain.wren` so the next guy doesn't
repeat my hour.

### 3. Statements in a method body

While bisecting I tried `static t3() { var x = ...` on a new line:

```
Compilation Error _app/domain line 28 Error at 'var': Expected expression.
Compilation Error _app/domain line 28 Error at 'x': Expect '}' at end of block.
```

So statement-style bodies are barely a thing — a method body after a newline
is treated as a definition list and `var` is not an expression. Constructors
with assignments are the exception (they're parsed as initializers). This
means: **a method body that starts on a new line basically can't do anything
useful.** Everything must be `{ expression }` on one line, or use explicit
`return` + one expression per line. Bizarre for someone used to blocks, but
you learn to live with it.

### 4. `--version` starts a server

`bialet --version` ignored the flag and started serving on port 7002. The
documented flag is `-v` (`bialet -v` → `bialet 0.12.0`). The long form just
silently runs the server. Minor, but a CLI that half-accepts flags is a CLI
that hangs your shell.

## The multi-form CSRF test (the important one)

My page has **9 forms** on it: 1 add form + 4 tasks × (toggle + delete). All
nine carry `{{ session.csrf }}`. I extracted every hidden token from the
rendered HTML:

```
$ grep -oP '(?<=name="_bialet_csrf" value=")[^"]+' page.html | sort | uniq -c
      9 dctlBMJPsG8Bpkc9h9DoT4xzxvvOj5Hu1Ja5nMMKbQ8BAwra1wja4CaUfMYr
```

**All 9 forms, one token.** Then I cross-fired them:

- **First form's token** (the add form) submitted to `/toggle` for task 1 →
  `302`, task flipped 0→1. **Works.**
- **Last form's token** (task 4's delete form) submitted to `/toggle` for
  task 2 → `302`, task flipped 0→1. **Works.**
- **No token** → `302`, task untouched.
- **Wrong token** (`WRONGTOKEN123`) → `302`, task untouched.
- **Cross-session attack** — a fresh cookie jar's token POSTed with the
  victim's session cookie → `302`, task untouched.
- **Token from before a server restart** still validated after restart
  (token lives in the DB, not memory).

Why it works, from the source (`src/bialet.wren`, `Session` class, lines
264-300):

```wren
csrf {
  var token = get("_bialet_csrf")          // cached in this instance
  if (!token) {
    token = Util.randomString(60)
    set("_bialet_csrf", token)             // REPLACE INTO ... (id,key)
  }
  return HtmlNode.new('<input ...>')
}
csrfOk { Util.secureEquals(get("_bialet_csrf"), Request.post("_bialet_csrf")) }
```

The token is generated **once per session** and cached in the instance's
`__values`, so the 2nd through 9th `{{ session.csrf }}` on the page are no-ops
that return the same token. `set()` upserts via `REPLACE INTO` on the
`(id, key)` primary key, so there's exactly one `_bialet_csrf` row per session
— I confirmed one row in `BIALET_SESSION`. `csrfOk` compares against the
server-side copy with a constant-time XOR loop (`Util.secureEquals`). This is
exactly the behavior I was told used to be broken (non-deterministic reads,
duplicate rows, last-rendered-token-wins). **As of 0.12.0 it is fixed and I
verified it end to end.** The `(id, key)` primary key + `REPLACE` + the
`ORDER BY updatedAt DESC` read in the `Session.new()` constructor all line up
now. The docs even document the "older DBs without the PK are rebuilt on
startup" migration (`bialet.wren` lines 999-1007) — I saw the rebuild code
and it's real.

Cookie defaults confirmed on the wire:

```
Set-Cookie: BIALETSESSID=mN0T8TNxwo7uZRIsrhxx1GWFwRe4KmqBL7KbZmjM; SameSite=Lax; Path=/; HttpOnly
```

Exactly as documented. Secure is added only behind TLS.

## Escaping / XSS test

Added a task whose text is `<script>alert(1)</script>`. Rendered:

```html
<span class="title">&lt;script&gt;alert(1)&lt;/script&gt;</span>
```

**Escaped by default** — `{{ }}` auto-escapes `& < > " '`. I don't need
`htmlspecialchars` on every output; the template does it and I'd have to go
out of my way to opt into raw HTML (`.raw` / `HtmlNode`). The docs warn
against `.safe` inside `{{ }}` because it double-escapes. Even *my own error
string* got escaped (`Task text can&apos;t be empty.`). This is the single
best thing about the stack for a paranoid old man.

## The SQL injection story

Covered above, but to be explicit: backtick queries are prepared statements,
the compiler blocks string interpolation into them, and my injection-looking
payloads were stored as literal text and rendered escaped. The two escape
hatches the docs warn about (`ORDER BY` via `.order()` allow-list, `LIMIT`
via placeholders) are sensible. I trusted raw `UPDATE ... WHERE id = ?` and
it behaved exactly like PDO with prepared statements.

## Scorecard

| Area | Verdict | Notes |
|---|---|---|
| Setup | Good | One binary, zero config, DB + migrations auto on first run. `--version` silently starting a server is a wart. |
| Templates / HTML-in-Wren | Mixed | Inline HTML + auto-escaping is nice, but implicit-return-on-`{`-line and mandatory `construct new()` are undocumented traps. `map` callbacks are single-expression only. |
| DB layer | Good | Parameterized by construction, `.to(Class)` mapping is neat, migrations are simple. String-typed columns are annoying but documented. |
| Escaping / XSS | Excellent | Automatic, correct, verified. The framework's best feature. |
| Sessions + CSRF | Excellent | Stable token, deterministic reads, PK-keyed table, constant-time compare, multi-form verified. Fixes I was told about are real. |
| Error messages | Weak | Runtime errors go to the server log and the browser gets a cute generic 500. Silent nulls (implicit return) are the real killer. `bialet dev` / `BIALET_SHOW_ERRORS` exists but is buried. |
| Routing | Good | Files-per-URL like static files, `Request.get()` for query params, PRG trivial. No router to configure. |

## Concrete asks

1. **Document the implicit-return newline rule in `template.md`**, loudly, in
   the Model section where the silent null first bites. One sentence: "a
   method body is an expression body (implicit return) only when the
   expression starts on the same line as `{`; otherwise it returns null."
   Even better: have the compiler warn when a method body block ends without
   an explicit return — silent null is worse than a crash.
2. **Document that component classes need a declared `construct new()`**, or
   give `Foo.new()` on a constructor-less class a friendlier error than
   "metaclass does not implement 'new()'".
3. **Fix the table-name inconsistency in `database.md`**: it lists
   `BIALET_SESSIONS`, but the real table is `BIALET_SESSION` (what
   `security.md` and the source use). An hour of SQLite queries against the
   wrong name is easy to waste.
4. **Make the 500 page show the error during development.** The generic
   "Oops! Something broke." page plus a log line is exactly the PHP-2008
   workflow I wanted to escape. Surface `bialet dev` / `BIALET_SHOW_ERRORS`
   in `getting-started` instead of burying it in `errors.md`.
5. **`--version` and other long flags should not silently start a server.**
   Either reject them or map them to `-v` / help. A flag that boots a server
   on a default port is how you end up with orphan processes.
6. **Compile-time check for bare `Request.post(name)`.** The docs hammer the
   null guard on every page because a missing field crashes the request. A
   linter warning when `Request.post(...)` is used without `|| ""` or a null
   check would make the #1 crash cause impossible to ship.
7. Keep the auto-escaping default. Do not add opt-in escaping. It is the
   right call and the reason I'd use this over raw PHP today.

## Bottom line

Sessions, CSRF, escaping, and SQL injection are handled for me — and handled
correctly, verified with curl, not just read in docs. That's further than most
PHP 2008-era code I maintain. The cost is a language (Wren) with sharp edges
that are either undocumented (implicit return, constructor requirement) or
fragile (single-expression `map` bodies, statement bodies basically unusable).
Once I wrote explicit `return` and a blank constructor, the app was boring —
which is exactly what I want from a framework.

Final state after a from-scratch run (fresh DB): migration applied once,
GET 200, add/toggle/delete all 302, rows in the table match the actions.
App is working.
