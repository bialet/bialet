# Elena's Session Notes

29, frontend engineer, 7 years of React + TypeScript + Next.js. I live in
component land. This session is me trying to build a todo app in **Bialet
0.12.0** — a single C binary with the Wren language + SQLite embedded, no npm,
no build step, no router config. I brought my JSX muscle memory, Alpine.js, and
Tailwind CDNs. Here's what actually happened, in order. 🚲

---

## What I set up

I read the docs first: `getting-started`, `template`, `database`, `forms`,
`security`, `errors`, `live-reload`, `wren`, `reference`. My architecture, in
"React terms":

| Bialet file | My React translation |
|---|---|
| `todo/_app/components.wren` | my components (`TaskItem`, `Filters`, `Stats`) |
| `todo/_app/domain.wren` | the `Task` model / entity class |
| `todo/_app/migration.wren` | my schema / Prisma migration |
| `todo/_app/template.wren` | `layout.tsx` |
| `todo/index.wren` | the `/` page (list + add form) |
| `todo/toggle.wren`, `todo/delete.wren` | POST-only route handlers |

Component methods are `static` methods returning HTML strings. Models are Wren
classes over backtick-SQL. "Components" are real, but there are no props, no
`children`, no keys, and no `useState` — the "state" is the SQLite database.

I ran the server with the documented recipe and tested end to end with curl.

## What worked (surprisingly well)

- **Zero-config startup.** `bialet -p 7013 <appdir>` and it's running. DB
  (`_db.sqlite3`) created automatically on first run. Migration ran
  automatically. I did not touch a config file.
- **File-based routing.** `index.wren` → `/`, `toggle.wren` → `/toggle`.
  No router, no `app.get(...)`. It genuinely is "the filesystem is the route
  table." I kept reaching for a router that wasn't there, then relaxed.
- **Post/Redirect/Get.** `return Response.redirect("/")` after every POST gave
  clean `302` + `Location: /` responses. Refresh-safe, as promised.
- **Auto-escaping is ON and works.** This was my #1 fear (my muscle memory:
  "JSX escapes by default, does Bialet?"). Interpolating the string
  `<script>alert('xss')</script>` into a `<p>` rendered:
  `<p>&lt;script&gt;alert(&apos;xss&apos;)&lt;/script&gt;</p>`.
  It escapes in **attributes** too: interpolating that string into a
  `class="..."` produced `class="&lt;script&gt;..."`, and `"hi" & bye` in an
  input value became `&quot;hi&quot; &amp; bye`. Zero thought required, exactly
  like React. To emit intentional markup you must opt in with `.raw` /
  `HtmlNode` — the `dangerouslySetInnerHTML` equivalent. Good trade.
- **CSRF is genuinely simple.** `{{ session.csrf }}` in every form,
  `session.csrfOk` on every POST. Missing token → my 400; wrong token → my 400
  with "Bad CSRF token"; valid token → 302. Cookie is `HttpOnly` +
  `SameSite=Lax` automatically.
- **`&&` conditional blocks, ternaries in attributes, `map` for lists,
  multi-line HTML components** — all parsed and rendered. My JSX habits mostly
  survived the migration.
- **Custom elements with hyphens work** (`<my-element>`). A few years ago that
  would've been a fight; now it just parses.
- **Live reload** (with `BIALET_LIVE_RELOAD`): the `/_livereload` endpoint
  returns a version number and a polling `<script>` gets injected before
  `</body>`. Confirmed working.
- **In-browser error pages** (`BIALET_SHOW_ERRORS`): on a 500 you get a real
  page with error type, module, line number, and message. This is the closest
  thing to my React error overlay, and it's decent.

## What broke — exact messages

These are the verbatim errors I hit. The generic 500 page alone shows
`🚨 Internal Server Error / Oops! Something broke.` — the useful text only shows
in the server log or with `BIALET_SHOW_ERRORS` on.

1. **Nesting the same tag** (the big one):
   `Cannot nest <div> inside <div>: use a different tag for the inner element.`
   Also fires for `<form>` and even `<my-custom-tag>`. Any outermost tag cannot
   reappear at *any* nesting depth — until the tree starts with a *different*
   tag, then it's free. So `<div><div>x</div></div>` dies but
   `<section><div><div>x</div></div></section>` is fine. This is **the** parser
   fight. I had to design every component's outer tag around it.
2. **Uppercase tag**: `<MyElement>` →
   `Error at '<': Expected expression.` (confusing, but it does reject)
3. **Underscore in tag name**: `<my_component>` →
   `Unterminated HTML string.` (misleading message — it's an invalid name, not
   an unterminated string)
4. **Leading infix operator on a new line**:
   ```
   return <div>{{
     show
     && <b>X</b>
   }}</div>
   ```
   → `Error at '&&': Expect end of HTML interpolation.` The operator must end
   the previous line, exactly as plain Wren. This bit me once, then I got it.
5. **Map callback with multiple statements**: silently renders **empty**
   (`<ul></ul>`) with NO error. The docs warn about this ("the body must fit on
   the line after the `{`"), and it's a genuine footgun — my `<li>` just
   vanished with zero feedback. I fixed it by pre-computing before `map`.
6. **`null` safety is oversold.** `wren.md` claims "accessing a key or calling
   a method on null returns null instead of throwing." Reality: `null["key"]`
   is safe, `null.count` returns `0`, `null.toString` is safe — but calling an
   *undefined* method on null throws `Null does not implement 'something'.`
   → 500. So the "null is safe" story is only true for what Null happens to
   implement. (This is why `Request.post(...) || ""` matters.)

## The parser fights, verdict by JSX habit

**Accepted now:**
- Multi-line inline HTML components (my `TaskItem`, `Filters`, `Stats`)
- Ternaries in attributes/classes: `class="{{ task.finished ? "done" : "open" }}"`
- Ternaries with whole HTML strings as operands
- `&&` for conditional blocks and class fragments
- `map { |x| ... }` for lists, with multi-line item HTML
- Hyphenated custom tags `<my-element>`
- `<br />`, `<hr />`, `<input ... />` self-closing (with space)
- Interpolation into attribute values
- Nested same tags once a different outer tag starts the tree

**Rejected (with the errors above):**
- `<div><div>...` same-tag nesting
- `<MyElement>` uppercase
- `<my_component>` underscore
- Leading `&&` / ternary operator at the start of a line
- Multi-statement `map` callbacks (silently, not even an error)

**Docs say "no" but reality accepts:**
- Mismatched tags. `template.md` says `<div><span>Hello</div>` **fails**.
  Reality: it passes syntax check *and* serves `200` with the literal broken
  string `<div><span>Hello</div>` as the body. React throws
  `Closing tag </div> does not match opening tag <span>` — Bialet silently
  hands back malformed HTML. This is both a docs bug and a framework bug.
- `<br/>` without a space. Docs call it "Incorrect"; the parser accepts it and
  emits `<br/>` verbatim. Harmless in browsers, but the doc is wrong.

## Escaping verdict: Bialet vs React

**Bialet wins on the default.** `{{ }}` auto-escapes plain values everywhere —
element bodies and attribute values — with zero ceremony. My first instinct
("do I have to wrap every string in a safe/escape function?") was wrong; it's
just on. The escaping is `&`, `<`, `>`, `"`, `'` → `&amp; &lt; &gt; &quot;
&apos;`, which is the same character set React escapes.

Differences worth noting:
- React's model is "escape by default, opt out with `dangerouslySetInnerHTML`".
  Bialet's is "escape by default, opt out with `.raw` / `HtmlNode.new(...)`".
  Same shape, and `.raw` is honestly nicer to write than
  `dangerouslySetInnerHTML={{__html: ...}}`.
- I noticed the page `<title>` "Elena's Todo" came out as `Elena&apos;s Todo`.
  That's *correct* escaping (browsers render `&apos;` fine) but it surprised me
  to see my own app's title get entity-encoded — React doesn't escape `'` in
  text children in a way that's visible in `innerHTML`. Cosmetic, not a bug.
- Bialet's escaping gives no warnings. React at least yells about missing keys
  and gives hydration warnings; Bialet will silently emit `&amp;lt;` if you
  double-escape with `.safe`. The `template.md` pitfall covers this.

## Multi-form CSRF test (the important one)

My page has **7 forms**: the add form, plus toggle + delete for each of 3
task rows, every one with `{{ session.csrf }}`. Result:

- All 7 forms on the page carried the **identical token**:
  `2O5smEDw511JA8U4rqyW4kfrttFuGKFvL1p9QTmXb18q4nIzcFgvDCdOaLmp`
  (counted via `uniq -c`: `7` × same value).
- The token from the **first** form (add) successfully validated a `POST
  /toggle` → `302 Found`.
- The token from the **last** form (last row's delete) successfully validated a
  `POST /delete` → `302 Found`.
- Same token reused across separate POSTs and across requests under one session
  cookie — always valid.
- No token → `400`, wrong token → `400` ("Bad CSRF token"), GET on the POST-only
  handlers → `302` back to `/`.

So: **stable per-session token, not rotated per form.** That's the good
behavior — earlier forms stay valid after later forms render, and the same
hidden field is safe to paste into every form. I had to *discover* this by
experimenting; the docs don't state it explicitly.

## Scorecard

| Area | Score | Notes |
|---|---|---|
| Setup | 5/5 | One binary, no config, DB + migration auto-run. Frictionless. |
| Components / JSX-like DX | 3/5 | Component methods compose fine, but no props/children/keys, no reactivity, and the single-expression `map` callback is a silent data-loss trap. |
| HTML parser | 3/5 | Fast, and the same-tag-nesting error is clear. But it silently accepts mismatched tags (broken HTML out), and the uppercase/underscore tag errors are misleading. |
| Escaping / XSS story | 4/5 | Auto-escape by default everywhere, like React. No dev-time warnings, so misuse is silent. |
| DB layer | 4/5 | Backtick SQL + `?` placeholders + `.to(Class)` mapping + auto migrations is genuinely nice. Values-as-strings is a permanent footgun (`Num.fromString` everywhere). |
| Error messages | 3/5 | With `BIALET_SHOW_ERRORS` on, compile/runtime errors show type + file + line. Without it you get a generic 🚨 page and must read the server log. |
| Tooling / live-reload / editor | 2/5 | Live reload works (1s polling). No language server, no VS Code support, no autocomplete/typechecking; `bialet -t` *executes* the file (runs migrations, and `Session.new()` throws `Null does not implement 'add(_)'` in that context). |

## Concrete asks (numbered changes that would fix my experience)

1. **Mismatched closing tags must be a compile error.** Docs already promise
   this; reality serves raw malformed HTML with a 200. This is the single
   worst surprise of the session.
2. **Better invalid-tag-name errors.** `Error at '<': Expected expression.`
   (uppercase) and `Unterminated HTML string.` (underscore) should say
   `Invalid tag name '<MyElement>': must be lowercase alphanumeric + hyphens.`
3. **Multi-statement `map` callbacks should throw**, not silently render empty
   `<ul></ul>`. Silent data loss is worse than a loud error.
4. **A VS Code extension / language server**, even a minimal one: hover docs
   for `Request`/`Session`/`Query`, real-time syntax checking, and a
   "check this file" that does *not* execute it (so `Session.new()` doesn't
   blow up outside a request).
5. **Fix the docs where they're wrong/misleading:**
   - `template.md`: mismatched tags *do not* fail (see #1).
   - `wren.md`: "null is safe" is overstated — undefined method calls on null
     throw `Null does not implement 'something'.`
   - `security.md` opening line ("there is no magic that escapes your output
     for you") contradicts the next section (auto-escaping on). Pick one story.
   - `live-reload.md` / `errors.md`: `BIALET_LIVE_RELOAD` and
     `BIALET_SHOW_ERRORS` are read once at startup — document that a restart
     is required after `Config.enable(...)`.
   - `BIALET_PROMPT.md`'s todo example omits CSRF entirely; every state-changing
     form needs `{{ session.csrf }}` + `session.csrfOk` per `security.md`.
6. **State explicitly that the CSRF token is stable across all forms on a
   page** (per session, not per form). I had to test 7 forms to find out it
   was safe to rely on.
7. **Emit `&#x27;` instead of `&apos;`** (cosmetic — matches React/HTML5
   convention and my diff-noise threshold), or at least document the choice.
8. **Dev-mode 500 page should show the error by default** in `bialet dev`
   (I know `dev` does this — I just want the standalone flag to be more
   discoverable than a buried `-r 'Config.enable(...)'` incantation).

## Bottom line

I built the whole thing — add, toggle, delete, filter, CSRF on every form,
Post/Redirect/Get — and it works end to end (verified with curl: `200` GET,
`302` POSTs, XSS escaped, CSRF rejected without a token, filters switch the
Alpine state, `_app/*` and `_db.sqlite3` return 403). Once I made peace with
"there's no component tree, the page re-renders server-side on every action,"
it felt like a very small, very honest PHP. I could ship this for a tiny
internal tool and enjoy it. I would not want to build a stateful SPA here.
The gap is DX polish (editor, diagnostics, the silent traps) — not capability.
