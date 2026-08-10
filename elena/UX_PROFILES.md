# Bialet UX Profiles: Three Critical Walkthroughs (dev 0.12.0)

A re-run of the persona study against the **latest `origin/dev`** build
(bialet 0.12.0). The previous run was against an older dev that had no
auto-escaping, a session table without a primary key, rotating CSRF tokens,
and hyphenless tag names. This run re-builds the same three persona apps from
scratch, using only the docs and the binary, to see which friction is gone and
what remains.

It is deliberately critical. Each profile ends with a severity-ranked list of
friction points and the concrete changes that would remove them. Everything
below was reproduced against the binary, not read off the roadmap.

- Profile 1: Maya — The Student (HTML & CSS)
- Profile 2: Carlos — The Old-School PHP Developer
- Profile 3: Elena — The React/TypeScript Frontend Engineer
- Cross-cutting findings
- Verified: what the previous run found broken and is now fixed
- Verified: what is still broken
- Recommended fixes (in order of impact)

---

## Profile 1: Maya, the CS Student

**Background.** 22, first full-stack project (a personal blog + guestbook).
Comfortable with HTML/CSS, has written a little JavaScript in class, terrified
of the modern toolchain. Bialet's promise — "No NPM, no YAML, no separate
database servers" — is exactly what she wants to hear.

### The journey

**Minutes 0–5: install.** The one-liner works, `bialet -v` → `bialet 0.12.0`,
and `index.wren` + `return <p>Hello World!</p>` + the server = a page. No
`npm init`, no config files. This is still the strongest part of the whole
experience. Two small walls in the first five minutes: `bialet --version`
(the long flag she instinctively tried) *starts a server on port 7001*
instead of printing a version, and `bialet dev` tries to open a text-mode
browser and prompts for a "terminal type" on a headless box.

**Minutes 5–30: the first page.** She writes HTML inside Wren, which feels
like magic. Then she hits the parser:

- A plain `<div>` wrapper containing another `<div>` fails to compile:
  `Cannot nest <div> inside <div>: use a different tag for the inner element.`
  followed by a confusing secondary error `Error at 'div': Expect end of
  file.` The rule is documented, and the error message now suggests the fix —
  but the rule itself still does not exist in HTML, and a list of cards is the
  most ordinary thing on the internet. She lost ~10 minutes.
- Uppercase tags give `Error at '<': Expected expression.` and underscore
  tags give `Unterminated HTML string.` Neither message mentions tag names.
  A beginner cannot map either error to "tag names must be lowercase,
  alphanumeric, and hyphenated."
- `<br/>` and `<br />` both work. The docs claim `<br/>` is "Incorrect."
  The doc is wrong. She tested it.
- Mismatched tags (`<div><span>Hello</div>`) compile and serve 200 with the
  broken HTML verbatim. The docs claim this "fails." The doc is wrong.

**Minutes 30–90: the todo list.** The single-file app works, and then the data
model bites her the same way it did before:

- `Request.post("msg")` returns `null` when a key is missing. Forget the
  `|| ""` once and the request 500s. This time the server log gives a good
  message (`Runtime Error Null does not implement 'trim'.` with file and
  line) — but the **browser** still shows a generic "🚨 Internal Server
  Error / Oops! Something broke." with no line, no file, no hint. The
  detailed error page exists (`BIALET_SHOW_ERRORS` / `bialet dev`) but it is
  not the default, and enabling it requires a server restart the docs don't
  mention.
- Database values come back as **strings**. `task["done"]` is `"0"` or `"1"`,
  and both are truthy in Wren, so her first `if (task["done"])` toggle
  silently did nothing.
- `""` is truthy. `if (text)` is not a presence check.
- A multi-statement `map` callback (`{ |i| var x = i; <li>...</li> }`)
  silently renders an empty `<ul></ul>` with no error at all.
- The docs drilled the `Request.post(...) || ""` rule into her so hard that
  she stopped tripping it — that repetition works.

**Minutes 90+.** She discovers there is a full `docs/examples/todo/` example,
but it is split across 7 files and not linked from the examples page. Her
editor (VS Code) shows `.wren` as plain text — no highlighting, no extension,
no LSP anywhere. `bialet -t` exists as a syntax checker, but it does not
catch mismatched tags and it executes the file it checks.

**The live-reload saga.** `live-reload.md` promises a polling script that
reloads the browser when files change. It is a lie in this build: the script
is injected and `GET /_livereload` returns a number, but **the number never
changes** when files are created, modified, or deleted. The Wren hot-reload
(save → next request serves new code) works, but the advertised browser
auto-reload is dead. She would blame her setup, restart everything, and it
still would not work.

### The "aha!" moment, checked honestly

The aha is different this time. The old one — "I built a full-stack app in a
single file" — is still real, but now it is joined by a second one that
landed harder: **she pasted `<script>alert(1)</script>` into a task on
purpose and the page showed it as literal text.** Escaping that is on by
default, with zero ceremony, is the thing that made her trust the framework.

The anti-aha has shifted too. The XSS panic is gone; the walls now are
silent failures (empty `<ul>` from a map bug, a 302-with-body when she
forgot `return`, live reload that never reloads) and a generic 500 page that
sends her to a log file.

---

## Profile 2: Carlos, the Old-School PHP Developer

**Background.** 47, 20 years of vanilla PHP + MySQL, classic MVC, files-per-URL,
`htmlspecialchars()` on everything. Distrusts magic; reads the source when
the docs get vague.

### The journey

**What worked first try:** `if (Request.isPost) { ...; return
Response.redirect("/") }` — every POST returned 302 + Location, PRG just
works. Parameterized backtick queries (`?` placeholders) — the compiler
refuses string interpolation into a backtick, so the injection path is closed
by construction. He verified: `'; DROP TABLE tasks; --` stored literally and
rendered escaped. `.to(Task)` mapping is neat, and `Null.to` is a no-op so
`find()` returns null safely. `_`/`.`-prefixed files return 403. The DB and
the migration auto-created on first run.

**Escaping / XSS — the best feature.** `{{ }}` auto-escapes `& < > " '` in
text and attributes. He did not need `htmlspecialchars` once, and would have
to go out of his way (`.raw` / `HtmlNode`) to opt into raw output. His own
error string ("Task text can't be empty.") came back as `can&apos;t`. He
called it the reason he would use this over raw PHP today.

**Sessions + CSRF — fixed, and he verified it.** His page has 9 forms (add +
4 tasks × toggle + delete), each with `{{ session.csrf }}`. He extracted
every token: all 9 were identical. He cross-fired them — first form's token
on `/toggle`, last form's token on `/toggle`, no token, wrong token,
cross-session token — and every legitimate combination worked, every invalid
one failed closed. He read `src/bialet.wren` to confirm why: `csrf`
generates **one token per session**, cached in the instance; `set()` is a
`REPLACE INTO` on the `PRIMARY KEY (id, key)` session table; `Session.new()`
reads with `ORDER BY updatedAt DESC`. He verified the old-DB rebuild code in
`Db.init` is real. The multi-form CSRF breakage from the previous run is
**fixed end to end**. He confirmed one `_bialet_csrf` row per session in
`BIALET_SESSION`.

**The walls.**

1. **No implicit constructor.** `class Template { layout(content) {...} }`
   then `Template.new().layout(...)` — exactly the docs' shape — blew up at
   runtime with `Runtime Error Template metaclass does not implement 'new()'.`
   Wren has no implicit constructor; the docs' example happens to declare
   `construct new()`, but nowhere says it is mandatory.
2. **The silent-null method body.** His `list()` method returned 0 rows while
   the identical expression at top level returned 4. No error, no log. He
   read `wren_compiler.c` to find why: a method body is an implicit-return
   "expression body" **only when the expression starts on the same line as
   the `{`**. Put a newline after `{` and it becomes a statement body that
   returns `null`. The docs ("A Wren block that contains a single expression
   implicitly returns that expression") never warn that the newline kills it.
   This cost him an hour.
3. **Statement-style method bodies barely work.** `static t() { var x = ...
   }` on a new line → `Error at 'var': Expected expression.` Combined with #2,
   a method body that starts on a new line can do almost nothing useful
   without an explicit `return`.
4. **`--version` starts a server.** Only `-v` prints the version; the long
   flag silently boots the server on port 7002.
5. **`-t` executes the file.** A file with `Session.new()` at top level
   threw `Runtime Error Null does not implement 'add(_)'.` under `-t`, which
   then still printed `✓ Syntax OK`.
6. **`database.md` names the session table `BIALET_SESSIONS`.** The real
   table is `BIALET_SESSION` (what `security.md` and the source use).
7. **Generic 500 page.** Runtime errors go to the server log; the browser
   gets "Oops! Something broke." `bialet dev` / `BIALET_SHOW_ERRORS` exists
   but is buried in `errors.md` and needs a restart.

**Scorecard verdict:** Setup good; templates/HTML-in-Wren mixed (nice inline
HTML + auto-escaping, but undocumented implicit-return and constructor
traps); DB layer good (parameterized by construction, `.to(Class)` mapping,
simple migrations); escaping/XSS excellent; sessions+CSRF excellent and
verified; error messages weak; routing good (files-per-URL, PRG trivial).

---

## Profile 3: Elena, the React/TypeScript Frontend Engineer

**Background.** 29, 7 years React + TS + Next.js. Lives in component land:
props, state, useEffect, JSX, Tailwind. Expects the framework to escape by
default because React does.

### The journey

**What worked.** Zero-config startup; file-based routing (index.wren → `/`,
toggle.wren → `/toggle`); PRG; auto-escaping **on by default, in text and
attributes** — her #1 fear ("do I have to wrap every string?") was wrong, it
is just on, and `.raw` / `HtmlNode` are the `dangerouslySetInnerHTML`
equivalent, nicer to write. CSRF is simple and the multi-form token behavior
is good (she stress-tested 7 forms — all identical tokens, first and last
both validated, no token → 400, wrong token → 400). `&&` conditional blocks,
ternaries in attributes, ternaries with HTML operands, multi-line
components, `map` lists, and **hyphenated custom elements `<my-element>`**
all parse — most of her JSX muscle memory survived. In-browser error pages
with `BIALET_SHOW_ERRORS` exist (type + module + line). Live reload: she
confirmed the script is injected and the endpoint returns a version — she
did *not* notice the version never changes.

**The parser fights.** The same-tag nesting rule is the big one: `Cannot nest
<div> inside <div>` (also fires for `<form>` and `<my-custom-tag>`); the
outermost tag cannot reappear at any depth until a different tag starts the
tree. She had to design every component's outer tag around it. Uppercase and
underscore tags produce misleading errors (`Expected expression.` /
`Unterminated HTML string.`).

**Where Bialet is worse than React.**

1. **Mismatched closing tags silently pass.** React throws `Closing tag
   </div> does not match opening tag <span>`. Bialet compiles it, serves 200,
   and hands back malformed HTML verbatim. The docs even promise it fails.
   Her single worst surprise.
2. **Multi-statement `map` callbacks silently render nothing.** `<ul></ul>`,
   zero error. React would at least be loud about an unexpected return.
3. **"null is safe" is oversold.** `wren.md` says "accessing a key or calling
   a method on null returns null instead of throwing." Reality: `null["key"]`
   and `null.count` are safe, but an *undefined* method on null throws
   `Null does not implement 'something'.` → 500. The safety story is only
   true for what `Null` happens to implement.
4. **No dev-time warnings.** Escaping gives no feedback; double-escaping with
   `.safe` silently emits `&amp;lt;`.
5. **`bialet -t` executes the file.** `Session.new()` blew up under `-t`,
   outside a request. No way to just *check* a file.
6. **Tooling.** No language server, no VS Code support, no autocomplete. She
   also hit the doc contradictions (`template.md` mismatched-tags and `<br/>`
   claims; `security.md`'s intro "no magic that escapes your output for you"
   directly contradicting its own auto-escaping rules; `BIALET_SHOW_ERRORS` /
   `BIALET_LIVE_RELOAD` being read once at startup; the `BIALET_PROMPT.md`
   example shipping a todo app with no CSRF at all).

**Scorecard verdict:** Setup 5/5; components/JSX-like DX 3/5 (compose fine,
no props/children/keys, single-expression map is a silent data-loss trap);
HTML parser 3/5 (clear same-tag error, silently accepts mismatched tags,
misleading invalid-name errors); escaping 4/5 (default-on like React, no
warnings); DB layer 4/5 (values-as-strings is a permanent footgun); error
messages 3/5; tooling 2/5. Bottom line: once she made peace with
"there's no component tree, the page re-renders server-side," it felt like a
very small, very honest PHP she could ship a tiny internal tool with — but
not a stateful SPA.

---

## Cross-cutting findings (all three, reproduced)

1. **The docs no longer match the binary in several places**, and every one
   of them cost a persona time: mismatched tags are claimed to fail but
   silently serve broken HTML (`template.md`); `<br/>` is claimed incorrect
   but works (`template.md`); forgotten `return` before redirect is claimed
   to produce a "double-response error" but actually sends a 302-with-body
   (`template.md`); the session table is named `BIALET_SESSIONS` in
   `database.md` but is `BIALET_SESSION`; `wren.md` claims null is "safe" in
   a way that is only partially true; `security.md` opens with "there is no
   magic that escapes your output for you" and then describes default-on
   escaping. Misleading pitfalls are worse than no pitfalls.
2. **The Wren language's sharp edges are now the biggest trap**, not the
   framework's security story. Silent nulls — the implicit-return newline
   rule, the single-expression `map` callback, single-line `if` blocks that
   reject `return` — produce empty output or no-op code with zero feedback.
   Three of the four walls Carlos hit, two of Maya's, and two of Elena's
   were silent Wren behavior. A PHP/JS/React dev cannot predict any of them.
3. **Auto-escaping on by default is unanimously the best feature.** All three
   personas mentioned it unprompted as the reason they would trust the
   framework. Do not regress it.
4. **Session/CSRF is now trustworthy.** Both developers stress-tested
   multi-form pages and both confirmed stable tokens, deterministic reads,
   and a keyed table. The documented workaround from the previous run (call
   `session.csrf` once per page) is no longer necessary — but the docs still
   do not state that the token is stable across forms on a page.
5. **The browser 500 page is still a wall.** Everyone hit a real bug and the
   browser showed "🚨 Internal Server Error / Oops! Something broke." The
   useful error is in the server log or behind `BIALET_SHOW_ERRORS`, which is
   not the default, is buried in `errors.md`, and needs a restart to enable.
6. **No editor support, still.** No VS Code extension, no LSP, no syntax
   highlighting. `.wren` files are plain text. The only tool is `-t`, which
   executes the file and misses mismatched tags.
7. **The parser still rejects legal HTML** (same-tag nesting) and its
   invalid-tag-name errors are misleading. Hyphens are now allowed and
   `<br/>` works, so two of the old parser complaints are gone.
8. **CSRF is not surfaced to beginners.** Maya built the whole app with no
   CSRF because she does not know what CSRF is and nothing in the
   getting-started path makes her care. The docs assume the threat model is
   already understood.

---

## Verified: what the previous run found broken, now fixed

The previous run's top findings were reproduced against `./build/bialet`
before this run and re-tested against dev 0.12.0:

1. **Multi-form CSRF — FIXED.** Previously only the LAST form's token on a
   page validated (`{{ session.csrf }}` rotated the stored token on every
   call). Now `csrf` generates one token per session, cached in the instance;
   all forms on a page carry the identical token, and the first, middle, and
   last form tokens all validate. Reproduced: 3 forms → identical tokens,
   form A valid, form C valid.
2. **`BIALET_SESSION` primary key — FIXED.** Schema is now
   `PRIMARY KEY (id, key)`; `REPLACE INTO` replaces instead of appending;
   older un-keyed databases are rebuilt in place on startup. No more unbounded
   session growth.
3. **`Session.get()` determinism — FIXED.** The constructor reads with
   `ORDER BY updatedAt DESC`; no more "last row iterated wins" flakiness.
4. **Auto-escaping — SHIPPED.** `{{ }}` escapes `& < > " '` by default in
   text and attributes. `.safe` is now a footgun (double-escapes) rather than
   the required opt-in, and the docs warn about it.
5. **Hyphens in tag names — SHIPPED.** `<my-element>` parses.
6. **CSRF token entropy — IMPROVED.** Tokens now come from the OS CSPRNG.

## Verified: what is still broken on dev 0.12.0

1. **Browser live-reload is dead.** The injected polling script polls
   `/_livereload` forever because the version never changes on file create /
   modify / delete. Verified with real content edits. The Wren hot-reload
   still works; the advertised browser auto-reload does not.
2. **Mismatched closing tags are accepted.** Compiles and serves 200 with raw
   malformed HTML. The docs promise a compile error. Both a docs bug and a
   framework bug.
3. **Wren's implicit-return newline rule.** A method body whose expression
   starts on a new line after `{` silently returns null. Silent data loss,
   undocumented.
4. **No implicit constructor.** `Template.new()` on a class without
   `construct new() {}` fails at runtime with a confusing metaclass error.
5. **`bialet -t` executes the file** and prints `✓ Syntax OK` even after a
   runtime error; it also does not catch mismatched tags.
6. **`bialet --version` starts a server** on port 7001 instead of printing a
   version. Only `-v` works.
7. **`BIALET_SHOW_ERRORS` and `BIALET_LIVE_RELOAD` are read once at startup.**
   Enabling them while the server runs does nothing until restart;
   undocumented.
8. **The default 500 page is a dead end.** Generic "Oops, something broke";
   the real error (which has file and line) is server-log-only unless dev
   error display is enabled.
9. **The same-tag nesting rule persists.** `<div>` in `<div>` still fails to
   compile, though the error now suggests a fix. `docs/template.md`
   documents the workaround.
10. **Misleading invalid-tag-name errors.** Uppercase → `Expected expression.`;
    underscore → `Unterminated HTML string.`
11. **Doc-vs-reality drift.** `<br/>` "incorrect" (works), mismatched tags
    "fail" (don't), "double-response error" (never happens), `BIALET_SESSIONS`
    (real: `BIALET_SESSION`), "null is safe" (only partially), security.md
    intro contradiction.
12. **Silent single-expression rules.** Multi-statement `map` callbacks and
    one-line `if { return ... }` blocks fail silently or with cryptic errors.

---

## Recommended fixes (in order of impact)

1. **Fix `/_livereload` so the version bumps on file change** — or remove
   the injected script. A polling script that never reloads is a broken
   promise and a silent dead feature. (One-line: recompute the version in the
   handler from the watch state.)
2. **Make mismatched closing tags a compile error.** The docs already promise
   it, React does it, and all three personas tripped on the silent acceptance.
3. **Document — and ideally warn about — the implicit-return newline rule.**
   One sentence in `template.md`'s Model section ("a method body is an
   expression body only when the expression starts on the same line as the
   `{`"), plus a compiler warning when a method body block ends without an
   explicit return. Silent null is worse than a crash.
4. **Reconcile the docs with 0.12.0 reality:** fix the mismatched-tags and
   `<br/>` claims in `template.md`, the "double-response error" claim, the
   `BIALET_SESSIONS` table name in `database.md`, the overstated "null is
   safe" in `wren.md`, and the `security.md` intro that contradicts
   auto-escaping.
5. **Make `-t` a real checker:** do not execute the file (so `Session.new()`
   cannot blow up), catch mismatched tags, and document that it must be run
   from inside the app directory.
6. **Show real errors in the browser during development by default**, and
   document that `BIALET_SHOW_ERRORS` / `BIALET_LIVE_RELOAD` are read at
   startup and need a restart.
7. **`bialet --version` and other long flags should not silently start a
   server.** Reject or map them; a flag that boots a server on a default port
   is how orphan processes happen.
8. **State explicitly that the CSRF token is stable across all forms on a
   page** (per session, not per form) in `docs/security.md`.
9. **Add even minimal editor support** — a VS Code extension with syntax
   highlighting, or an LSP, or at least mention one in the docs.
10. **Better invalid-tag-name errors** ("tag names must be lowercase
    alphanumeric + hyphens") instead of `Expected expression.` / `Unterminated
    HTML string.`
11. **Give beginners a CSRF nudge** — a one-line note on the forms page:
    "any form that changes data should include `{{ session.csrf }}` and check
    `session.csrfOk`; this is called CSRF."
12. **Link `docs/examples/todo/` from the examples page** (or delete it);
    all three personas either never found it or found it by accident.

---

## One-paragraph verdict

The re-run confirms the core trade — one binary, filesystem routing, SQLite,
inline HTML — and shows the framework's security story is now the real deal:
auto-escaping on by default, parameterized SQL by construction, and a
session/CSRF layer that survives a 9-form page, all verified empirically. The
friction moved. What drives the personas away now is not security but *silent
behavior*: a method body that quietly returns null, a `map` callback that
quietly renders nothing, mismatched tags that quietly serve broken HTML, a
live-reload script that quietly polls forever, a `-t` checker that quietly
executes your file, and docs that quietly disagree with the binary. Fix the
silence — a compile error for mismatched tags, a warning for implicit-return
nulls, a working version bump for `/_livereload`, and docs that match 0.12.0 —
and Bialet stops being "great for the happy path" and becomes "safe to hand
to real people, including the ones who don't know what a statement body is."
