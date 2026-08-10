# Maya's Session Notes

I'm Maya, 22, CS student. I've written HTML and CSS for years, a little
JavaScript in a class, and exactly zero server-side code before today. My
worst nightmare is a `node_modules` folder with 40,000 packages in it. I came
to Bialet because it promised "no NPM, no YAML, no separate database servers —
just a tiny binary." This is everything that happened, in order, including the
parts where I got stuck and wanted to cry.

---

## Reading the docs (~45 min)

I read the getting-started tutorial first, then template.md, database.md,
forms.md, errors.md, live-reload.md, faq.md, and skimmed the README.

Confused by:
- `getting-started.md` is just a redirect page. Fine, whatever, Sphinx thing.
- The tutorial builds a **poll app**, not a todo app. I had to translate
  "radio button vote" into "add/delete a task" in my head. Not hard, but the
  docs never have a todo app example in the main flow. I found out later there
  IS one buried in `docs/examples/todo/` but it's split across 7 files and it's
  not even listed on the examples page. I never would have found it.
- `_app.wren` and classes and imports felt like a lot. The tutorial says to put
  a `Template` class in `_app.wren` and import it everywhere. I understood the
  *words* but I couldn't picture why my first app needs an MVC split. I'm
  building one page. I decided to keep it in one file — the README's guestbook
  example does exactly that and it looked like the thing I wanted.
- Migrations: the tutorial tells you to create `_migration.wren`. The README
  instead just runs `CREATE TABLE IF NOT EXISTS` at the top of a page and it
  just works every request. I went with the README. "It re-runs every time but
  it's a no-op, so it's free" is *exactly* the kind of hack I understand.
- `Request.post("name")` returns `null` and can crash. This warning is in
  getting-started, forms.md, template.md, the reference, AND the FAQ. I got it.
  It's almost annoying how many times it's repeated, but honestly it saved me.

Really liked:
- The "check → process → redirect" form pattern is stated clearly and repeated.
  I could copy it without thinking.
- `{{ }}` escapes automatically — this is stated over and over with XSS
  examples. Great for my anxiety level.
- The tone. It's the first programming doc that felt like it was written for a
  person who is scared. The "Pitfall" callouts are where I actually learned.

## First page (~10 min)

Docs say create `index.wren` with `return <p>Hello World!</p>`, then run
`bialet dev`. I typed it, and… the server started, then it tried to open a
text browser (I'm on a headless box) and asked me for a "terminal type". That
was a real "wait, what?" moment. The `dev` command works — it said "Live
reloading" and "Showing errors in browser" — but the browser-opening part is
very much designed for a human with a real desktop. I got past it by running
the server directly:

```
nohup /tmp/opencode/bialet-dev/build/bialet -p 7011 ./todo >/tmp/maya.log 2>&1 &
```

Version check: `bialet -v` → `bialet 0.12.0`. Good, there's a version flag.
(The README's usage block lists `-v`; nothing else about it. Fine.)

Stuck points here:
- `bialet --version` (my first instinct) just started a server and I had to
  kill it. The README documents `-v`, but nobody tells you the "no-args"
  behavior is "serve the current directory on port 7001." A beginner typing
  `bialet` to check the version gets a server they don't know how to stop.
  (~2 min of flailing.)
- I tried `pkill -f bialet` once and my whole terminal froze. Turns out that
  pattern kills the shell too. That's a me-problem, not a Bialet-problem, but
  it's worth writing down: I eventually killed servers by `kill <pid>`.

The "Hello World" itself worked first try. That was the moment I got excited.

## Deliberate experiments (this is what I was told to do — try things)

I'm told to test edge cases instead of trusting the docs. I did. These are the
REAL results, not what the docs say:

1. **`<div>` inside `<div>`** — the docs warn about this ("Cannot nest <div>
   inside <div>"). I tried it:
   ```
   Compilation Error ... line 1 Error: Cannot nest <div> inside <div>: use a different tag for the inner element.
   Compilation Error ... line 1 Error at 'div': Expect end of file.
   ```
   The first line is actually pretty helpful. The second line ("Expect end of
   file") is nonsense to a beginner. More importantly: **this rule is insane for
   someone who knows HTML.** Nested divs are the most normal thing in the
   world. I hit it for real when I tried to make a reusable "card" chunk with a
   `<div>` wrapper around inner `<div>`s. Cost me ~10 minutes of starring at
   my own HTML before I remembered the docs. The workaround (make the outer
   tag `<main>` or `<section>`) is fine, but this is my #1 complaint.

2. **`<br/>` vs `<br />`** — the docs say `<br/>` is INCORRECT and you must
   write `<br />`, and that the slash gets omitted anyway. I tested both.
   **Both work. Identically.** `<br/>` and `<br />` both pass the syntax
   checker and both render exactly as written. The docs are just wrong on this
   one. I'm honestly relieved — but it makes me wonder what else is stale.

3. **Mismatched tags** — docs say `<div><span>Hello</div>` "fails." I tested
   it: passes the syntax checker AND runs, producing the mismatched HTML
   verbatim. The browser fixes it. So the doc's claim is false in 0.12.0. The
   advice "match your tags" is good advice, but the docs say it will error and
   it doesn't.

4. **`<custom-tag>` with a hyphen** — works, as documented. Nice.

5. **Uppercase / underscore tags** — fail as documented, but with confusing
   messages:
   - `<MyElement>` → `Error at '<': Expected expression.`
   - `<my_component>` → `Unterminated HTML string.`
   Neither message tells you "tag names must be lowercase letters, numbers,
   and hyphens." I only understood because I'd read it. A beginner gets
   "Expected expression" and has no idea what they did wrong.

6. **Escaping** — the big one. I interpolated `"<script>alert('xss')</script>"`
   as a task text and as an attribute value. Both came back escaped:
   ```
   &lt;script&gt;alert(&apos;xss&apos;)&lt;/script&gt;
   ```
   This is a REAL feature that works. I was genuinely impressed. It's the
   first framework where I don't have to remember to escape — it's default-on.
   I didn't even have to think about XSS; it just didn't happen.

7. **Conditional `&&` in a class attribute** — `class="x {{ count > 1 && "active" }}"`
   → `class="x active"`. Works, and reads fine. Good.

## Building the actual todo app (~45 min, including the 10-minute div meltdown)

Single file `index.wren`, plus `style.css`. Three features: add, toggle
done/undone, delete. Post/Redirect/Get everywhere.

The structure I landed on: all three forms POST to `/` and say what they want
with a hidden `<input name="action" value="add|toggle|delete">`. The controller
switches on `Request.post("action")`, does the SQL, then
`return Response.redirect("/")`.

What bit me, in order:

1. **The div nesting thing** (see above). This was real. When I first built
   the list item as `<div class="todo-item"><div>…</div></div>` it died with
   the nest error. Fix: use `<li>` for items (it's a list! my HTML instinct
   was wrong) and `<main>` for the page shell.

2. **Forgetting `return` before `Response.redirect()`** — the docs scream
   about this ("double-response error"). I actually did it while refactoring.
   The result was NOT an error. The server sent `302` with my page body glued
   onto it, no log message at all. Browsers ignore 302 bodies so it "worked",
   but silently. The docs describe a crash that doesn't happen in 0.12.0. That
   one confused me for a few minutes because I was *expecting* an error and got
   a weird-but-working response. (~5 min)

3. **`Request.post("text")` null crash** — I wrote `Request.post("text").trim`
   without the `|| ""` shield at one point. GET (no POST) → 500. Browser shows
   only "🚨 Internal Server Error / Oops! Something broke." The real message
   is buried in the server terminal:
   ```
   Runtime Error Null does not implement 'trim'.
   Stack Error /tmp/.../todo/index.wren line 2 (script)
   ```
   Two findings here. (a) The message itself is good and tells you the file and
   line. (b) **It is not shown in the browser by default.** I had to go read a
   log file. errors.md explains this and gives a dev mode, but a beginner's
   first instinct is "the browser said something broke and nothing else."

4. **DB values are strings** — the docs warn about this and they're right.
   `task["done"]` is `"0"`/`"1"`, not a boolean. My first toggle did
   `if (task["done"])` which is always truthy (both "0" and "1" are non-empty
   strings in Wren). The toggle just never worked. The fix was comparing to the
   string `"0"`. (~5 min, but I was very confused when "0" was truthy.)

5. **Multi-statement `map` callback renders nothing** — I tried to declare a
   variable inside a `map` block to build a class string:
   ```
   {{ items.map { |i|
     var x = i
     <li>{{ i }}</li>
   } }}
   ```
   Result: `<ul></ul>`. Empty. **No error at all.** The docs do mention this
   ("the block returns null and the output is empty") but silent empty output
   is the worst kind of bug for a beginner — I spent a while wondering if my
   query returned nothing. Workaround per docs: inline ternaries, or precompute
   before the `map`.

6. **`-t` syntax checker quirk** — I used `bialet -t file` to check files
   before running (great idea, documented in README). It kept failing with
   `Error: app directory not found` or `Error: Cannot resolve file` until I
   realized I must run it **from inside the app directory** and pass the file
   name, not a path. The tool is genuinely useful — it prints
   `✓ Syntax OK` — but its path resolution is not documented anywhere. And
   remember: it does NOT catch mismatched tags. (~5 min)

## What genuinely impressed me (the good parts)

- **One file, one page, zero config.** I created the table by pasting
  `CREATE TABLE IF NOT EXISTS` at the top of my page, ran the server, and a
  `_db.sqlite3` file just appeared next to my code. No database server, no
  connection string, no ORM. That is the entire reason I came here and it
  delivered.
- **The escaping.** Pasting `<script>` into a task and watching it come back
  as text made me trust the framework. That's the sort of default safety I
  didn't know I was allowed to have.
- **The error page in dev mode.** `errors.md` documents
  `Config.enable("BIALET_SHOW_ERRORS")` (or just `bialet dev`). Once on, a
  500 shows in the browser:
  ```
  Runtime Error: Null does not implement 'trim'.
  Stack Error: /tmp/.../todo/index.wren line 2: (script)
  ```
  File and line, right in the page. That's when the "Oops, something broke"
  wall became an actual usable tool. (One gotcha: the flag is read at server
  **startup** — I set it while the server was running, got nothing, and had to
  restart. The docs don't mention that.)
- **Wren file hot-reload.** I changed the `<h1>` text, hit refresh in curl,
  and the new text was there. No restart, no build. On Linux it just watches
  the `.wren` files. I assumed I'd need a restart or a "reload" button.
- **PRG works.** One POST add, then five refreshes → still exactly one task.
  The redirect-based no-resubmit pattern behaves exactly like the docs say.
- **Private files.** `/_db.sqlite3` and anything else starting with `_` or `.`
  returns 403. I didn't have to configure a single thing to keep my database
  off the internet. Defaults being safe is huge for a beginner.
- **Static CSS.** `style.css` just worked, served with the right content type.
  CSS in Bialet is CSS.

## The live reload saga (broken in this build)

`live-reload.md` describes a polling script injected into every page that hits
`/_livereload` and reloads the browser when a version number changes. I
enabled it (`Config.enable("BIALET_LIVE_RELOAD")`), restarted, and confirmed:

1. The script IS injected before `</body>`:
   ```
   <script>(function(){var v=null;setInterval(...GET /_livereload ...})()</script>
   ```
2. `GET /_livereload` returns a version number. So far so good.
3. **The version number never changes.** I edited files, created new files,
   deleted files — version stayed `1786392401` every single time. The docs say
   "updates whenever a file in the app directory changes." It doesn't. In this
   build the injected script would poll forever and never reload the page.

So: the Wren VM hot-reload (save file → next request serves new code) works,
but the **browser auto-reload is dead on arrival** in 0.12.0. I'd have noticed
as a user, because I was promised "the browser reloads automatically" and it
just… didn't. I'd blame my setup, restart everything, and it still wouldn't
work. That's exactly the kind of silent failure that eats a beginner's
afternoon.

## Other little things I noticed

- The 404 page is friendly: "⚠️ Not found / Uh-oh! No route found." The
  default 500 is "🚨 Internal Server Error / Oops! Something broke." Cute, but
  useless without the dev error page.
- `Could not bind port 7011 - Check if the port is already in use.` — good,
  clear message.
- `-r 'Config.enable("BIALET_SHOW_ERRORS")'` needs a valid one-statement Wren
  snippet. I wrote a `System.print(...)` with a `;` first and got
  `Error: Invalid character ';'.` (I didn't even know that was wrong.)
- The double slash in request logs: `Request GET //break_null` (harmless
  cosmetic thing).
- **CSRF**: I saw the word "CSRF" in forms.md while skimming and had no idea
  what it was, so I scrolled past it. I built all three forms without a token.
  That's apparently "expected" for this exercise, but it's worth saying out
  loud: the docs mention CSRF in one login-form example and one checklist, and
  a beginner who doesn't know what CSRF *is* will not search for it. The docs
  assume you know the threat model already.
- Editor support: zero. `.wren` files are plain text in VS Code, no
  highlighting, no plugin mentioned anywhere in the docs I read. My safety net
  was the `-t` checker, which is good but catches less than you'd hope (see
  mismatched tags).

## Scorecard

| Category | Score | Notes |
|---|---|---|
| Install / zero-config | 9/10 | One binary. DB file auto-appears. Table created from a one-line SQL pasted in my page. The only friction was accidentally starting a server with `--version` and the headless `dev` browser-open prompt. |
| "Use your HTML skills" | 7/10 | HTML inside Wren is genuinely just HTML. But the no-nested-same-tag rule and the space-in-`<br />` / mismatch warnings (that turned out false) made me distrust my HTML instincts. |
| Error messages | 5/10 | Great *log* messages (`Null does not implement 'trim'`, file + line). Terrible *default browser* experience (generic "Oops, something broke"). Dev error page fixes it but needs a restart to enable. |
| Beginner safety | 8/10 | Auto-escaping, parameterized queries hammered into every doc, private files by default. But silent failures (`<ul></ul>` from a map bug, 302-with-body) and no CSRF guard rails at all. |
| Editor support | 2/10 | Nothing. No highlighting, no plugin mentioned, no docs on it. The `-t` CLI checker is the only help and it's undocumented-in-detail and misses mismatched tags. |
| Live reload | 3/10 | Wren hot-reload works great; the documented browser auto-reload is broken in this build (version never changes). |
| The aha moment | — | Watching a `<script>` string come back escaped. A framework that is safe by *default*. That, plus the DB file just existing, is why I'd use this for my class project. |

## Concrete asks (what would fix my experience)

1. **Kill or fix the nested-same-tag rule.** Either let the outer tag repeat
   when it's a *variable-length* inline string (the browser handles nesting
   fine), or make the error tell me the workaround directly: "use a different
   wrapper tag, e.g. wrap in `<section>`." And drop the secondary
   "Expect end of file" error that accompanies it — it's noise.

2. **Fix the docs vs. 0.12.0 reality:** `<br/>` works; mismatched tags do NOT
   error; forgetting `return` before redirect does NOT produce a
   "double-response error." These three documented pitfalls are wrong in this
   build. Misleading pitfalls are worse than no pitfalls.

3. **Show real errors in the browser by default in dev.** The generic 500 page
   sent me to a log file. Flip on the detailed error page whenever running
   without a flag like `--prod`, or at least print "see server log for error"
   with the actual message. And document that `BIALET_SHOW_ERRORS` needs a
   server restart.

4. **Fix `/_livereload`** so the version actually changes when files change —
   the entire browser auto-reload feature is dead in 0.12.0. If it can't be
   fixed soon, remove the injected script so I don't think I have a feature I
   don't have.

5. **Make `map` callback failures loud.** When a block callback returns null
   and renders empty, log a warning ("map callback returned null — did you
   declare a variable inside the block?"). Silent `<ul></ul>` cost me ten
   minutes.

6. **Document the `-t` checker properly.** Say it must be run from inside the
   app directory, and state clearly what it does NOT check (inner tag
   matching). Maybe add a VS Code extension or a `make check-syntax` style
   helper.

7. **A beginner-visible nudge for CSRF.** A one-line note on the form pattern
   page: "any form that changes data should include `{{ session.csrf }}` and
   check `session.csrfOk` — this is called CSRF, here's a 2-minute explanation."
   Right now the docs assume I already know the word.

8. **The examples page doesn't list the todo example.** `docs/examples/todo/`
   exists and is split across 7 files but isn't linked from the examples page.
   Either link it or delete it; discovering it by accident made me question
   whether I was building "the Bialet way."
