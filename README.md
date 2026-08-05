# Personas — Bialet UX Research

A role-played evaluation of Bialet through three target users. Each persona
built the same simple **todo list** app, from scratch, using only the
public docs and the actual binary. Their code, their in-file comments, and
their session notes are preserved here exactly as they "wrote" them — quirks
included.

This is deliberately critical. The goal is to find the friction points that
drive each persona away, not to praise the happy path.

## The personas

| Persona | Folder | Background | Built |
|---|---|---|---|
| **Maya** — The Student | `personas/maya/` | HTML/CSS, first backend project | single-file `index.wren` todo |
| **Carlos** — The PHP Dev | `personas/carlos/` | 20 yrs vanilla PHP + MySQL | MVC-style todo with model, migrations, CSRF |
| **Elena** — The React Dev | `personas/elena/` | React + TS + Next.js | component-style todo with Alpine.js + Tailwind |

## What each folder contains

- `UX_PROFILES.md` — the analysis of that persona's likely experience
- `NOTES.md` — the persona's own first-person session log (what they hit, in
  order, in their voice)
- `todo/` — the app they built. It is a runnable Bialet app:
  `bialet -p 7001 personas/<name>/todo` (see below)

## Verified findings (reproduced against the binary)

All three personas hit the same walls, and the CSRF findings were reproduced
empirically against `./build/bialet`:

1. **Multi-form CSRF is broken.** With `{{ session.csrf }}` in more than one
   form on a page, only the LAST form's token validates; every earlier form
   fails `csrfOk`. Reproduced: 3 forms → submit first = FAIL, middle = FAIL,
   last = OK.
2. **`BIALET_SESSION` has no primary key.** `CREATE TABLE IF NOT EXISTS
   BIALET_SESSION (id TEXT, key TEXT, val TEXT, updatedAt DATETIME)`. Because
   nothing is unique, `REPLACE INTO` appends a row on every write. A session
   accumulates one `_bialet_csrf` row per page load, forever.
3. **`Session.get()` is non-deterministic.** It loads every row for the
   session with no `ORDER BY` and keeps the last one iterated. With two
   token rows, the same request sequence was observed to both pass and fail
   CSRF. Workaround (used in the Carlos/Elena apps): generate the token once
   per page and reuse the same hidden field in every form.
4. **No auto-escaping.** `{{ value }}` is raw HTML; `.safe` is opt-in. All
   three personas either shipped XSS or narrowly avoided it.
5. **The HTML parser rejects legal HTML** — `<div>` inside `<div>` fails;
   hyphens are illegal in tag names. Maya and Elena both hit this in their
   first half hour.
6. **A server timing quirk**: back-to-back requests can produce a different
   CSRF outcome than the same sequence with a small delay (see #3).

## Running the apps

```bash
# Maya (no CSRF, single file)
./build/bialet -p 7001 personas/maya/todo

# Carlos (MVC + CSRF)
./build/bialet -p 7002 personas/carlos/todo

# Elena (components + Alpine + Tailwind)
./build/bialet -p 7003 personas/elena/todo
```

Each app creates its own `_db.sqlite3` on first run (gitignored).

## Quick reproduction of the multi-form CSRF bug

```wren
var s = Session.new()
if (Request.isPost) return s.csrfOk ? "OK" : "FAIL"
return <main>
  <form method="post">{{ s.csrf }}<button>A</button></form>
  <form method="post">{{ s.csrf }}<button>B</button></form>
  <form method="post">{{ s.csrf }}<button>C</button></form>
</main>
```

Serve it, load the page, submit form A's token: `FAIL`. Submit form C's
token: `OK`. The stored token is whatever the last `csrf` call wrote, and
`get()` returns "some" row because the table has no key and the query has no
order.
