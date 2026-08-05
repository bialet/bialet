# Bialet UX Profiles: Three Critical Walkthroughs

This document walks three hypothetical users through Bialet — a student, a
veteran PHP developer, and a React/TypeScript engineer — and reconstructs their
experience with the framework step by step, from first install to pushing past
the tutorial.

It is deliberately critical. Each profile ends with a severity-ranked list of
friction points and the concrete changes that would remove them. Everything
below is grounded in the current behavior described in `docs/` and the
codebase, not in speculation about future versions.

- Profile 1: Maya — The Student (HTML & CSS)
- Profile 2: Carlos — The Old-School PHP Developer
- Profile 3: Elena — The React/TypeScript Frontend Engineer
- Cross-cutting findings

---

## Profile 1: Maya, the CS Student

**Background.** 22, first full-stack project (a personal blog + guestbook).
Comfortable with HTML/CSS, has written a little JavaScript in class, terrified
of the modern toolchain. Bialet's promise — "No NPM, no YAML, no separate
database servers" — is exactly what she wants to hear.

### The journey

**Minutes 0–5: install.** The one-liner works on her Mac (Apple Silicon
binary). She is told to create `index.wren` and run `bialet`. This is the
strongest part of the whole experience: no `npm init`, no `npx`, no config
files, one port, one browser tab. The welcome page appears.

**Minutes 5–30: the first page.** She writes HTML inside Wren, which feels
like magic. Then she hits the parser.

- A plain `<div>` wrapper containing another `<div>` fails to compile. The
  outermost tag of an inline HTML string cannot repeat at any nesting level
  (`docs/template.md`). She needs `<div>` and a list of cards, so she reaches
  for exactly the pattern the parser rejects.
- `<br/>` fails; `<br />` is required. That is a silent, arbitrary-looking
  rule to a beginner.
- Tag names cannot contain hyphens. She names a component `<custom-tag>` and
  gets a parse error. CSS custom properties and web components both use
  hyphens; Wren tells her they're illegal.
- Interpolation depth is capped at 9 levels. She will hit this while nesting
  ternaries, because the docs actively teach nested ternaries.

None of these rules exist in HTML. She believes she already knows HTML, and
the tool is now telling her she doesn't. This is the first moment the "use
your HTML skills" pitch breaks.

**Minutes 30–90: the guestbook.** She follows the README example and gets
something working. Then the data model bites her:

- `Request.post("msg")` returns `null` when a key is missing. Calling a
  string method on `null` crashes the request. The docs' fix is `|| ""` on
  every single field. She forgets it once, gets a 500, and the error page is a
  generic "Internal server error" with no line number and no hint
  (`docs/errors.md`).
- Database values come back as **strings**. Her vote counter does string
  concatenation instead of addition. She only finds out because she reads the
  docs; nothing tells her at runtime.
- `""` is truthy in Wren. The forms guide itself warns: `if (q)` does not
  work as a presence check; you need `if (q != "")`. To a student, `""` being
  truthy is just wrong.
- She must remember `.safe` on every user-supplied string or she ships an XSS
  hole. There is no auto-escaping. A beginner *will* forget it, and nothing
  in the framework catches it. The roadmap acknowledges this ("Auto HTML
  escaping" is a longer-term item, `ROADMAP.md`), which means the project
  knows the default is dangerous.

**Minutes 90+: going further.** She wants an edit page for a blog post, i.e.
`/article?id=42`. The docs push query strings over path segments
(`docs/advanced-routing.md`), which is a fine mental model for PHP veterans
but unfamiliar to her. She has never used `$_GET`. The SEO-friendly URL she
imagined requires a `_route.wren` escape hatch she has to learn separately.

She tries to debug with `System.print(...)` on the server stdout — in a
separate terminal, with no structured errors, no stack trace formatting, and
no source line. If she is on macOS, her live-reload is broken too: reload uses
inotify (Linux only) and the FAQ tells her to restart the server manually.
The "see your changes instantly" promise silently dies on the most common
student laptop.

She opens VS Code, the editor every course told her to use, and there is no
syntax highlighting, no snippets, no diagnostics — Vim/Neovim have support,
VS Code does not (`ROADMAP.md`). The "wren" files render as plain text.

### The "aha!" moment, checked honestly

The advertised aha — "I built a full-stack app in a single file" — is real and
it survives contact. One `.wren` file with a query, a POST handler, and inline
HTML is genuinely astonishing to someone who expected React + Node + a
database. That moment is the product.

But it is followed immediately by the anti-aha: the app she built is
vulnerable (forgot `.safe`), the HTML parser rejects her valid HTML, and the
errors give her no path forward. She got the demo, then hit a wall.

### Friction, ranked

| # | Friction | Severity | Why it hurts |
|---|----------|----------|--------------|
| 1 | No auto-escaping; `.safe` opt-in | Critical | Ships XSS by default; beginners cannot be expected to self-audit |
| 2 | HTML parser rejects valid HTML (same-tag nesting, hyphen tags, `<br/>`) | High | Contradicts the "use your HTML skills" promise at minute 5 |
| 3 | Generic 500 page, no line numbers, no hints | High | Dead-end debugging for someone with no runtime experience |
| 4 | No VS Code extension (only Vim/Neovim) | High | The default student editor has zero support |
| 5 | Live reload is Linux-only; manual restart on macOS | High | Kills the "instant feedback" loop on the most common student machine |
| 6 | `Request.post()` → `null` crash-by-default | High | One forgotten `\|\| ""` is a 500 with a blank message |
| 7 | DB values as strings; `""` truthy; no runtime hints | Medium | Data bugs that are invisible until logic runs |
| 8 | Query-string-first routing | Medium | An unfamiliar model for someone who never saw `$_GET` |
| 9 | No scaffold (`bialet new`), no online playground | Medium | First-file friction; FAQ admits no try-online exists |
| 10 | New dialect of Wren, not standard Wren | Low | Small extra tax on top of "you must learn a language" |

### Recommended changes for Maya

1. **Flip escaping to safe-by-default.** Make `{{ value }}` escape, add
   `{{ value.raw }}` for trusted markup. This is already on the roadmap — move
   it up. It is the single highest-impact change for every profile below.
2. **Ship a VS Code extension** (syntax + language-config + snippets) before
   any other DX work. She lives in VS Code.
3. **A dev-only error page** showing file, line, and the offending source line
   with a one-line hint ("did you forget `.safe`?", "did you guard the POST
   field with `|| \"\"`?"). The knowledge is already in the docs; surface it
   at the moment of failure.
4. **Loosen or remove the same-tag nesting rule.** It exists for the parser,
   but it makes the tool lie about HTML. At minimum, detect the pattern and
   suggest the workaround in the error message.
5. **`bialet new <name>`** scaffold that generates `index.wren`, a layout,
   and a working guestbook so her first file is not blank.
6. **macOS live reload** (kqueue) — or, failing that, stop advertising live
   reload on the homepage.
7. **A classroom onboarding path**: a 10-minute "no SQL, no Wren beyond a
   loop" tutorial where the DB is invisible, then reveal it.

---

## Profile 2: Carlos, the PHP Freelancer

**Background.** 45, twenty years of vanilla PHP + MySQL on shared hosting.
Built a hundred small business sites. Skeptical of Laravel, allergic to
JAMstack and Docker. Bialet's manifesto — "HTML is the Real Frontend,"
"Standards, not frameworks" — speaks his language.

### The journey

**Setup.** He runs `curl | sh` on his Linux VPS. No `composer install`, no
`php-fpm`, no `nginx` config, no `.htaccess` gymnastics. He drops a `.wren`
file in a folder, runs the binary, and gets a page. The file-to-URL mapping is
PHP's model exactly — `about.wren` is `/about`, the filesystem is the router
(`docs/advanced-routing.md`). He understands this instantly. This is the best
possible first ten minutes.

**The language swap.** He knows PHP cold and now has to write Wren. The syntax
is C/JS/Python-familiar, and he can read it fine. What he *can't* do is reuse
any of the 20 years of PHP snippets, Composer packages, or muscle memory. His
freelance business runs on `include` + a utility folder. Bialet's answer is
`import` plus remote `gh:` imports — but there is no package registry, no
version resolution, and cached remote modules never update until you
manually clear the `BIALET_REMOTE_MODULES` table (`docs/advanced-routing.md`).
For a paid client project, silently pinning `main` forever is a liability.

**The CRM.** He builds a small client CRM. Raw SQL with `?` placeholders maps
onto his PDO reflexes; prepared statements that reject interpolation are
actually stricter than PHP and he approves. `save()` that inspects the `id`
field to decide INSERT vs UPDATE is a neat, predictable mini-ORM. This part is
a genuine win.

Then the edges:

- **No real auth story.** Sessions + `Util.hash`/`Util.verify` exist, but
  `Util.hash` is salted SHA-256 — a fast hash the security docs themselves say
  is only "fine for internal tools" and recommends a slow KDF for real users
  (`docs/security.md`). His clients ask for "login for our staff and let them
  add users." There is no OAuth, no MFA, no roles, no admin scaffold. A PHP
  dev expects a CMS or framework to hand him an admin panel; he has to build
  auth, admin, and validation from scratch.
- **No native TLS.** Bialet speaks HTTP/1.0 and needs a reverse proxy for
  HTTPS (`docs/security.md`). On his shared-hosting setup — cPanel, existing
  Apache, auto-SSL — that means he must run the binary on a random port and
  proxy to it, which cPanel hosts make annoying. He could install it on a VPS
  with systemd + nginx, but his *existing* clients live on shared hosts where
  he has no shell. The single-binary pitch quietly assumes he owns a box.
- **Windows.** He develops on Windows. The cross-compiled binary requires
  three DLLs shipped alongside it (`libsqlite3-0.dll`,
  `libcrypto-3-x64.dll`, `libssl-3-x64.dll`, per `README.md`). His `make
  install` muscle memory from Linux does not apply.
- **Migrations are name-based.** `Db.migrate("Name", query)` tracks by name
  and has no rollback, no `down()`, no version numbers. Fine for one-man
  projects; he has clients who ask "can you revert last week's change?" The
  answer today is a manual restore.
- **Concurrency and lock contention.** One process, SQLite. For a 50-employee
  CRM it's fine. He'll hit the wall only when a client's "small tool" grows
  into the business — and the FAQ's answer is basically "go use a different
  framework," which is honest but is a real ceiling on his revenue per client.
- **Long-term maintainability.** He is 45 and thinks about the next developer
  who inherits this code. There is no Wren talent pool, no Stack Overflow
  mass, no one to hand the project to. That is the single hardest objection
  for him, and it is rational.

### The "aha!" moment, checked honestly

The manifesto lands. "Ride Light. Simplicity is a superpower" is written by
someone who shares his disgust with the modern stack. He will evangelize the
binary deploy to other freelancers.

But the honest verdict: Bialet is PHP for people who are **already on a Linux
VPS**. Carlos's actual business is built on shared hosting and Windows dev
machines, and the tool's deployment story assumes the opposite in both cases.

### Friction, ranked

| # | Friction | Severity | Why it hurts |
|---|----------|----------|--------------|
| 1 | No shared-hosting path (needs reverse proxy + shell) | Critical | His entire existing client base is unserved |
| 2 | No native TLS (HTTP/1.0, proxy required) | High | Kills direct-domain deployment for small clients |
| 3 | No admin panel, no roles, weak default password hashing | High | He has to rebuild what CMS users take for granted |
| 4 | Niche language, no ecosystem, no talent pool | High | Rational long-term objection he will raise to himself |
| 5 | Windows builds need DLL juggling | Medium | His dev machine is Windows; setup friction |
| 6 | Remote imports pin `main` and never update | Medium | Supply-chain uncertainty on paid work |
| 7 | Name-based migrations, no rollback | Medium | No path to undo a bad schema change |
| 8 | Single-process ceiling | Low | Fine now, a real revenue ceiling later |

### Recommended changes for Carlos

1. **A shared-host deploy story.** Either a no-shell, port-based "drop
   binary + app folder into a cPanel-style docroot behind existing Apache
   (mod_proxy_fcgi or .htaccess rewrite)" recipe, or explicit docs that Bialet
   requires a VPS and stop suggesting otherwise. Be open about it like the
   rest of the docs are.
2. **Native TLS** so a domain can point at the binary without nginx. It's on
   the roadmap; it should be near-term. Also serve HTTP/1.1.
3. **An admin scaffold or an honest replacement.** A minimal generated CRUD
   admin over a table would cover most of his CRM work and is very much in
   Bialet's scope. Note the roadmap's admin dashboard is read-only (browse
   DB/logs) — generate, not just view.
4. **Document the auth upgrade path** (slow KDF via a proxy/auth layer, OAuth
   via `Http` outbound calls) so the security page is not a dead end.
5. **Single self-contained Windows binary** (statically link SQLite/OpenSSL
   for Windows like `make static` does for Linux).
6. **Make remote imports update-on-reload in dev**, or at least surface a
   warning when a pinned `main` import was cached more than N days ago.
7. **`Db.rollback`/versioned migrations** — even a "name → down-script"
   convention is better than nothing.

---

## Profile 3: Elena, the React Engineer

**Background.** 30, frontend engineer at a SaaS company, React + TypeScript +
Next.js daily. Burned out on bundler churn and `node_modules`. Side project: a
personal dashboard. Bialet's "anti-bloat" pitch is exactly her fantasy.

### The journey

**Setup.** She does the one-liner and, honestly, it's refreshing. No
`package.json`, no `next.config.js`, no build. The first page renders. She
feels the weight lifting.

**The template system.** Bialet's inline HTML is JSX-shaped, so she feels
immediately at home. Then the differences surface, one by one:

- **The same-tag nesting rule.** She writes a `<div>` wrapper containing
  `<div>` cards — the most common layout pattern in existence — and it fails.
  In JSX this is legal; here it's a parse error with a workaround that reads
  like a bug report (`docs/template.md`).
- **Map callbacks are single expressions.** She cannot declare a variable
  inside `tasks.map { |t| ... }`. The docs tell her to pre-compute before the
  template instead. That is a design constraint, not a language law, and it
  breaks a pattern she uses daily.
- **`{{ }}` newline rules.** The Wren expression must start on the same line
  as `{{`; only the inner HTML string can span lines. She writes
  multi-line JSX by instinct and gets silent empty output.
- **Interpolation depth cap of 9.** Deeply nested ternaries — which the docs
  encourage — hit it.

Every one of these is a JSX feature she relies on, removed or restricted.
JSX is a real template language; this is a string-literal parser with a
JSX coat of paint. The moment she composes components, she discovers the
differences, because **Bialet has no components** — only Wren methods that
return HTML strings, and the parser's own rules cap how they nest.

**The data and state model.**

- **No reactivity, no client components.** The docs are honest: classic
  multi-page app, forms and links, sprinkle Alpine.js. For a dashboard with
  live-ish data, she needs polling or Alpine — and Bialet has no WebSockets
  or SSE at all (`docs/why-bialet.md`, `docs/faq.md`). Her side project's
  central feature (live charts) is off the table without bolting on a
  separate service.
- **Fresh VM per request.** There is no process-global state to cache in.
  Every request recompiles and re-runs her script. She is told to put
  everything in SQLite. Coming from a world where she can memoize or cache
  in-process, this is a step backwards in tooling sophistication.
- **DB values are strings; `""` is truthy; `Request.post` is null-prone.**
  The same correctness traps as Maya, plus the docs' heavy reliance on
  `Num.fromString` everywhere makes her code louder than the equivalent
  TypeScript.
- **`.safe` everywhere.** React escapes by default. Here, forgetting one
  `.safe` is an XSS hole. Her React instincts — "interpolation is safe" —
  actively produce vulnerabilities. This is a regression against her training,
  and it is the worst-designed footgun in the framework.

**The DX.**

- **No TypeScript, no LSP, no diagnostics.** The roadmap has an LSP as a
  longer-term idea and VS Code support is unchecked. She opens a `.wren` file
  in VS Code and gets plain text. For an engineer whose entire workflow is
  editor-driven, this is disqualifying on its own.
- **Live reload exists on Linux only** and refreshes via polling, not a
  pushed WebSocket; the roadmap item is explicitly to add WebSocket
  live-reload. Her browser does not refresh, errors are not overlaid, and
  there's no dev-server error overlay at all.
- **No test story worth the name.** Integration tests exist (`tests/run.sh`,
  HTTP assertions). There is no unit-test runner, no fixture loading, no
  mocking. She cannot `npm test` her logic; she has to curl.
- **Tailwind CDN for dev, but "self-host for production."** The moment she
  wants a real Tailwind build she reintroduces the build step she fled. The
  framework is honest about it, but the "no build step" promise has a corner.

### The "aha!" moment, checked honestly

The "ultimate anti-bloat tool" feeling is real for about an hour. Then she
collides with the parser restrictions, the missing editor support, the
no-typing, and the manual escaping — and realizes she traded React's churn for
a smaller, harder wall. Bialet sells itself as "everything you need for a
data-driven site" and that's true; what it doesn't say is that it also
requires leaving behind everything that made JS tooling tolerable for her.

### Friction, ranked

| # | Friction | Severity | Why it hurts |
|---|----------|----------|--------------|
| 1 | No LSP / VS Code support / diagnostics | Critical | Her entire workflow is editor-driven |
| 2 | No auto-escaping; `.safe` opt-in | Critical | React training makes her write XSS by default |
| 3 | Template parser restricts JSX patterns she uses daily | High | Same-tag nesting, single-expression callbacks, newline rules, 9-level cap |
| 4 | No WebSockets/SSE, no reactivity | High | Live dashboards — her stated use case — are out of scope |
| 5 | No unit tests, fixtures, or test runner | High | She cannot verify logic without curling |
| 6 | No process-level caching; fresh VM per request | Medium | Reintroduces a problem modern frameworks solved |
| 7 | Live reload is Linux-only, polling, no browser refresh | Medium | The DX promise breaks on her Mac |
| 8 | `""` truthy, DB strings, null-prone POST fields | Medium | Correctness traps she stopped hitting in TS |
| 9 | No real component model | Medium | Composition is capped by parser rules |

### Recommended changes for Elena

1. **LSP + VS Code extension with live diagnostics.** This is the largest
   lever for her, and it would lift every other profile too. It should be a
   release goal, not a longer-term idea.
2. **Auto-escape by default** (`{{ }}` escapes; `{{ ... .raw }}` opts out).
   Same as Maya's #1. This is the one change that makes the framework safe to
   recommend to people whose instincts came from React.
3. **Relax the template parser to be JSX-like where it can be.** Allow
   multi-statement map callbacks, drop the same-tag nesting rule, raise or
   remove the 9-level cap. These are parser improvements, not scope creep.
4. **Document the real-time gap prominently.** A "When you need live data"
   page with concrete polling/Alpine/SSE-through-a-service recipes, so the
   disappointment happens in the docs, not at 2am.
5. **A unit-test story** — even a `bialet test` that runs a directory of
   `.wren` test files with assertions and a table-driven runner.
6. **A typed/checked mode** — at minimum a linter (unused variables, missing
   `.safe` on user input, missing `return` before redirect), since full TS is
   out of scope.
7. **Move WebSocket live-reload (roadmap 0.13) and opcode caching up.**

---

## Cross-cutting findings

Three problems dominate all three profiles, in different costumes:

**1. Escaping is backwards.** Every profile tripped on `.safe`. The roadmap
already lists "auto HTML escaping" as a longer-term idea — the maintainers
know. This should be the top engineering priority. Safe-by-default, with
explicit `raw` for trusted content. It is the difference between a toy and a
framework you can hand to people with dangerous instincts.

**2. The HTML parser is a tax on valid HTML.** The same-tag nesting rule,
hyphen-less tag names, `<br/>` vs `<br />`, 9-level interpolation caps, and
`{{ }}` newline rules are all parser constraints leaking into user
experience. They actively contradict the "HTML is the Real Frontend" pitch.
Every restriction should be either removed or turned into a friendly, specific
error the moment it happens.

**3. Editor support is the quiet blocker.** Vim/Neovim have syntax. The three
most likely new users (VS Code student, VS Code React dev) get nothing, and
there is no LSP. The roadmap treats this as an idea, not a release blocker.
For two of three profiles it is disqualifying.

Secondary themes, all present in the docs today and all worth their own
release items: live reload that works everywhere and actually refreshes the
browser; a dev-mode error page with file/line/hints instead of a generic 500;
explicit "not for your use case" guidance for auth, real-time, and shared
hosting; and a decision on the Windows story (self-contained binary or
honest DLL docs).

---

## Field findings: reproduced bugs (from building the persona apps)

Writing the three `todo/` apps against the real binary surfaced one severe,
reproducible defect in the session/CSRF layer that no profile predicted and
the docs never warn about. The persona session notes in `personas/` tell the
story in their own words; here is the technical summary.

### The session store is not a key-value store

`BIALET_SESSION` is created with no primary key and no unique index:

```sql
CREATE TABLE IF NOT EXISTS BIALET_SESSION (id TEXT, key TEXT, val TEXT, updatedAt DATETIME)
```

`Session.set()` writes with `REPLACE INTO`, which is a no-op for replacement
when nothing is unique — it just appends. Consequences:

- **Session rows accumulate forever.** Every `Session.set()` (including every
  `session.csrf` call, i.e. every page render that emits a form) inserts a new
  row. A session that renders a form 50 times has 50 rows for `_bialet_csrf`.
  `Db.clean` deletes expired sessions, not duplicate keys within a session.
- **`Session.get()` is non-deterministic.** The constructor loads *every* row
  for the session (`SELECT key, val ... WHERE id = ?`, no `ORDER BY`, no
  `LIMIT`) and writes each into a map — the last row iterated wins. Which row
  is "last" is whatever SQLite happens to return for an unordered query.
  Verified empirically: with two token rows present, the same request
  sequence both passed and failed `csrfOk` across runs.

### Multi-form CSRF is broken out of the box

The documented pattern — `{{ session.csrf }}` in every state-changing form —
rotates the stored token on every call. On a page with N forms there are N
different rendered tokens and only the last one stored. Verified:

```wren
var s = Session.new()
if (Request.isPost) return s.csrfOk ? "OK" : "FAIL"
return <main>
  <form method="post">{{ s.csrf }}<button>A</button></form>
  <form method="post">{{ s.csrf }}<button>B</button></form>
  <form method="post">{{ s.csrf }}<button>C</button></form>
</main>
```

Submit A's token → `FAIL`. Submit B's → `FAIL`. Submit C's → `OK`.

This is the single most damaging finding. A todo/CRM/dashboard — every app
with an add form plus per-row edit/delete forms — has multiple forms on one
page by definition, so the documented usage fails for all but the last form.

**Working workaround** (used in the Carlos and Elena apps, discovered by
reading the source): generate the token once and reuse the same hidden field
in every form.

```wren
var csrf = ""
if (!Request.isPost) {
  csrf = session.csrf          // called exactly once per page
}
// ... {{ csrf }} in every form ...
```

`csrfOk` still compares against the stored token; because only one token row
was written since the last page load, `get()` has a single candidate. (If a
session has accumulated rows from many earlier page loads, even this
workaround stays flaky — see the non-determinism above.)

### Recommended fixes (in order of impact)

1. `PRIMARY KEY (id, key)` on `BIALET_SESSION` (or `UNIQUE(id, key)`) so
   `REPLACE INTO` actually replaces. This is a one-line schema change with
   wide blast radius: it fixes unbounded session growth AND the stale-token
   reads.
2. Make `get()` deterministic: `SELECT key, val ... WHERE id = ? ORDER BY
   updatedAt DESC LIMIT 1` (or rely on the PK and use `INSERT OR REPLACE`).
3. Stop `Session.csrf` from rotating the stored token on render, or document
   the single-token-per-page rule loudly. Multi-form pages are the norm.
4. Document the bug and the workaround in `docs/security.md` today, so users
   stop hitting it blind. The security page claims "a token is generated per
   session, stored server-side, and rendered as a hidden form field" — it
   does not say "only the last form on the page validates."

---

## One-paragraph verdict

Bialet's core trade — one binary, filesystem routing, SQLite, inline HTML —
is sound and genuinely delightful for a narrow slice of work. But the three
users above all hit the same walls: unsafe-by-default escaping, an HTML
parser that rejects ordinary HTML, no editor support worth the name, a
generic 500 page with no path forward — and, worst of all, a session/CSRF
layer that silently breaks multi-form pages out of the box. The framework's
honesty about its limits is its best feature; the gaps are exactly the ones
its own roadmap has already identified. Fix the session table first (it's a
one-line schema fix that makes CSRF trustworthy), then escaping, then the
parser, then the editor story — and Bialet stops being "great for the happy
path" and becomes "safe to hand to real people."
