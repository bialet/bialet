# Elena's Session Notes — "JSX that isn't, a session store that isn't"

First-person log of building `todo/` with Bialet, as Elena would write it.
React + TypeScript + Next.js for a living, burned out on the toolchain.
The app itself is in `todo/`.

## What happened, in order

1. **The first ten minutes are great.** No `package.json`, no `next.config`,
   no bundler, one binary, first page renders. I felt the weight lift. This
   is the most honest "anti-bloat" pitch I've ever seen.

2. **Then the template system lied to me.** Bialet's inline HTML looks like
   JSX — until it isn't:

   - `<div>` inside `<div>` is a parse error. The most common layout pattern
     in existence. I flattened to `<li>` with a comment so I don't hit it
     again.
   - Map callbacks are single expressions. No variable declarations inside
     `tasks.map { |t| ... }`. I pre-compute before the template instead.
   - The Wren expression inside `{{ }}` must start on the same line as `{{`.
     I wrote multi-line JSX by instinct and got silent empty output — no
     error, just nothing.
   - Tag names can't have hyphens, so no web components.
   - Interpolation depth is capped at 9. Deeply nested ternaries — which the
     docs encourage — hit it.
   - **There are no components.** Just methods returning HTML strings, and
     the parser's own rules limit how they compose. I built a `TaskItem`
     "component" that returns an `<li>`; it works, but it's a method, not a
     component.

   Every one of these is a JSX feature I rely on daily, removed or
   restricted. JSX is a real template language; this is a string-literal
   parser wearing JSX's clothes.

3. **React trained me to write XSS.** React escapes by default. Bialet does
   NOT — `{{ value }}` is raw, and forgetting `.safe` is a hole. My muscle
   memory produced vulnerable code for an hour before the security page set
   me straight. This is the most dangerous default in the framework, and the
   roadmap item "auto HTML escaping" confirms the maintainers know it too.

4. **The CSRF mechanism is broken in ways the docs don't warn about.** I
   followed the docs (token in every form) and hit intermittent form
   failures. Digging in (source in `src/bialet.wren`):

   - `Session.csrf` generates a NEW token and stores it on every call. Two
     forms on a page = only the last one's token validates.
   - `BIALET_SESSION` is created with **no primary key**, so `REPLACE INTO`
     appends rows forever. Every page load adds a row; the table grows
     unbounded.
   - `Session.get()` loads every row for the session and the last one in
     SQLite's (unordered) result wins. The returned token is effectively
     arbitrary once you have two rows. I reproduced the SAME request
     sequence both passing and failing.
   - Workaround: generate the token once per page, reuse it in all forms.
     Not documented anywhere. I found it by reading the framework source.
   - There's also a timing-sensitive behavior: back-to-back requests can
     fail CSRF while the same sequence with a small delay succeeds. For a
     single-process, single-threaded server (it is — one `accept()` in a
     loop) this shouldn't be possible, which tells me the token read order
     is the real culprit, not concurrency.

5. **No live data.** My side project is a dashboard and Bialet has no
   WebSockets and no SSE — the docs are upfront about it. Polling or an
   external service is the only path. For a "personal dashboard" that wants
   live-ish charts, that's a hard ceiling.

6. **The DX is the quiet killer.** No TypeScript, no LSP, no diagnostics. I
   opened my `.wren` files in VS Code and got plain text. The roadmap lists
   LSP and VS Code support as ideas, not releases. For someone whose whole
   workflow is editor-driven, this is disqualifying on its own. Live reload
   is Linux-only, uses polling, and doesn't refresh the browser.

7. **Tailwind means I rebuild the build step I fled.** The docs say use the
   CDN in dev and "self-host for production" — which means running a Tailwind
   build pipeline against the app directory. So "no build step" has a corner
   that shows up exactly when the app gets serious.

## What I loved

- Zero config, zero `node_modules`, one binary. The relief is real.
- Alpine.js works in the HTML (I verified `@click`, `:class`, `x-show` all
  parse), so I could get client-side filtering without a build step.
- Raw SQL with prepared statements is a breath of fresh air after ORMs.
- The docs are honest about the limits. That's rare and I respect it.

## Scorecard (React/TS eyes)

| Aspect | Grade | Note |
|---|---|---|
| Zero config / install | A | genuinely refreshing |
| Template system | D | JSX-shaped but restricted; no components |
| Auto-escaping | F | `.safe` opt-in is a React-dev footgun |
| CSRF / sessions | F | multi-form breakage, no-PK table, arbitrary read order |
| Real-time (WS/SSE) | D | absent, honest but absent |
| Editor support | F | no LSP, no VS Code, plain text files |
| Live reload | D | Linux-only polling, no browser refresh |
| Typing / checks | F | no TS, no linter, no diagnostics |
| Testing | D | curl-based integration only; no unit test runner |

## Concrete asks

1. **LSP + a VS Code extension with live diagnostics.** This is the largest
   lever for me and it lifts every other profile too.
2. **Auto-escape by default** (`{{ }}` escapes; `{{ ... .raw }}` opts out).
   Same as everyone else's #1. Make it the next release, not a roadmap item.
3. **Relax the template parser to be JSX-like where it can be**: allow
   multi-statement map callbacks, drop the same-tag nesting rule, raise the
   9-level cap.
4. **Fix the session table** (`PRIMARY KEY (id, key)`) and make `get()`
   deterministic with `ORDER BY updatedAt DESC LIMIT 1`.
5. **A unit-test story** — even `bialet test` running a directory of `.wren`
   test files would beat "curl and grep".
6. **Move WebSocket live-reload and opcode caching up** the roadmap.
7. **A real component model or clear docs** on composing HTML — right now the
   parser rules cap composition in ways JSX never would.
