# Personas — Bialet UX Research (re-run, dev 0.12.0)

A role-played evaluation of Bialet through three target users. Each persona
built the same simple **todo list** app, from scratch, using only the
public docs and the actual binary built from the latest `origin/dev`
(bialet 0.12.0). Their code, their in-file comments, and their session notes
are preserved here exactly as they "wrote" them — quirks included.

This is a **re-run**. The first run happened against an older dev that lacked
auto-escaping, shipped a session table without a primary key, rotated CSRF
tokens on every form render, and rejected hyphens in tag names. This run
rebuilds the same apps against the current binary to see what improved and
what did not.

This is deliberately critical. The goal is to find the friction points that
drive each persona away, not to praise the happy path.

## The personas

| Persona | Folder | Background | Built |
|---|---|---|---|
| **Maya** — The Student | `maya/` | HTML/CSS, first backend project | single-file `index.wren` todo |
| **Carlos** — The PHP Dev | `carlos/` | 20 yrs vanilla PHP + MySQL | MVC-style todo with model, migrations, CSRF |
| **Elena** — The React Dev | `elena/` | React + TS + Next.js | component-style todo with Alpine.js + Tailwind |

## What each folder contains

- `UX_PROFILES.md` — the analysis of that persona's likely experience
- `NOTES.md` — the persona's own first-person session log (what they hit, in
  order, in their voice)
- `todo/` — the app they built. It is a runnable Bialet app:
  `bialet -p 7001 <name>/todo` (see below)

## What changed since the previous run (verified against the binary)

The previous run's top findings were re-tested against dev 0.12.0:

1. **Multi-form CSRF is FIXED.** `{{ session.csrf }}` now generates one token
   per session, cached in the instance. Every form on a page carries the
   identical token, and the first, middle, and last form tokens all validate.
   Reproduced: 3 forms → identical tokens; form A valid, form C valid.
   Previously only the LAST form's token validated.
2. **`BIALET_SESSION` has a primary key now** — `PRIMARY KEY (id, key)`.
   `REPLACE INTO` replaces instead of appending; older un-keyed databases are
   rebuilt in place on startup. No more one `_bialet_csrf` row per page load.
3. **`Session.get()` is deterministic** — `ORDER BY updatedAt DESC`.
4. **Auto-escaping is ON by default.** `{{ value }}` escapes `& < > " '` in
   text and attributes. `.safe` is now a footgun (double-escapes), not the
   required opt-in.
5. **Hyphens are allowed in tag names.** `<my-element>` parses.
6. **CSRF tokens come from the OS CSPRNG.**

## What is still broken on dev 0.12.0 (verified)

1. **Browser live-reload is dead.** The injected polling script polls
   `/_livereload` forever because the version number never changes on file
   create / modify / delete. Wren hot-reload still works.
2. **Mismatched closing tags are silently accepted** — compiles and serves
   200 with raw malformed HTML. The docs claim this fails.
3. **The Wren implicit-return newline rule.** A method body whose expression
   starts on a new line after `{` silently returns null (verified: expression
   body → 3 rows, statement body → 0 rows). Silent data loss, undocumented.
4. **`Template.new()` needs a declared `construct new()`** — no implicit
   constructor; runtime error otherwise.
5. **`bialet -t` executes the file** and prints `✓ Syntax OK` even after a
   runtime error; it does not catch mismatched tags.
6. **`bialet --version` starts a server** on port 7001. Only `-v` prints the
   version.
7. **`BIALET_SHOW_ERRORS` / `BIALET_LIVE_RELOAD` are read once at startup** —
   enabling while running requires a restart (undocumented).
8. **The default 500 page is a dead end.** The real error (with file + line)
   is server-log-only unless dev error display is on.
9. **The same-tag nesting rule persists** (`Cannot nest <div> inside <div>`),
   and invalid-tag-name errors are misleading (`Expected expression.` /
   `Unterminated HTML string.`).
10. **Docs drift from 0.12.0 reality:** `<br/>` is called "Incorrect" but
    works; mismatched tags are called a compile failure but aren't; the
    "double-response error" for a forgotten `return` never happens (302 with
    body instead); `database.md` names the session table `BIALET_SESSIONS`
    (real: `BIALET_SESSION`); `wren.md`'s "null is safe" is overstated;
    `security.md`'s intro ("no magic that escapes your output") contradicts
    its own auto-escaping rules.
11. **Silent single-expression rules.** Multi-statement `map` callbacks render
    empty `<ul></ul>` with no error.

Full detail, per-persona reproductions, and the severity-ranked fix list are
in `UX_PROFILES.md` (identical in each persona folder).

## Running the apps

```bash
# Maya (no CSRF, single file)
./build/bialet -p 7001 maya/todo

# Carlos (MVC + CSRF)
./build/bialet -p 7002 carlos/todo

# Elena (components + Alpine + Tailwind, CSRF)
./build/bialet -p 7003 elena/todo
```

Each app creates its own `_db.sqlite3` on first run (gitignored).

## Reproducing the fixed multi-form CSRF behavior

```wren
var s = Session.new()
if (Request.isPost) return s.csrfOk ? "OK" : "FAIL"
return <main>
  <form method="post">{{ s.csrf }}<button>A</button></form>
  <form method="post">{{ s.csrf }}<button>B</button></form>
  <form method="post">{{ s.csrf }}<button>C</button></form>
</main>
```

Serve it, load the page, submit form A's token: `OK`. Submit form C's token:
`OK`. All three rendered tokens are identical; the token is generated once
per session and `csrfOk` compares against it.
