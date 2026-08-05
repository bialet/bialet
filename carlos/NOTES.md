# Carlos's Session Notes — "It's PHP for people who own a VPS"

First-person log of building `todo/` with Bialet, as Carlos would write it.
20 years of vanilla PHP + MySQL, built a hundred small business sites.
The app itself is in `todo/`.

## What happened, in order

1. **The first ten minutes are the best ten minutes.** Drop a `.wren` file in
   a folder, run the binary, the file IS the URL. That's the PHP mental model
   exactly — I understood it instantly. No Composer, no php-fpm, no nginx
   config, no `.htaccess`. For an old hand this feels like coming home.

2. **Raw SQL with prepared statements — good.** `` `SELECT ... WHERE id = ?` ``
   with `?` placeholders is PDO, and the compiler REJECTING interpolation
   into a query is stricter than PHP. I trust it more than most PHP I've
   written. `save()` that inspects the `id` field to decide INSERT vs UPDATE
   is a tidy, predictable mini-ORM. This part is genuinely well designed.

3. **The CSRF mess is the worst thing I hit.** I followed the docs: put
   `{{ session.csrf }}` in every form. My add form worked when the list was
   empty, then started failing once I had rows on the page. I spent real
   time on this and traced it to three compounding problems:

   - **Multiple `{{ session.csrf }}` on one page = only the last one works.**
     Each call generates a new token and stores it. Every form before the
     last carries a stale token.
   - **`BIALET_SESSION` has no primary key.** The `CREATE TABLE` is
     `(id TEXT, key TEXT, val TEXT, updatedAt DATETIME)` — nothing unique.
     So `REPLACE INTO` never replaces; every token write APPENDS a row. My
     session table grows one row per page load, forever.
   - **`Session.get()` reads back "some" row.** The constructor does
     `SELECT key, val FROM BIALET_SESSION WHERE id = ?` with no `ORDER BY`
     and shoves every row into a map; the LAST one iterated wins. With two
     token rows, the returned token is whatever SQLite happens to hand back
     last — and I watched the same request sequence both pass and fail.
   - Workaround I settled on: generate the token ONCE per page and reuse the
     same hidden field in every form. That's not in the docs anywhere. I
     reverse-engineered it from the source.

4. **`Session.new()` at the top of every file** — in PHP I'd guard sessions
   with `session_status()`; here I call it and get a cookie back. Fine. But
   sessions are in the database (`BIALET_SESSION`), which means every request
   reads SQLite. For a small CRM that's fine; for anything bigger it's a
   bottleneck you can't opt out of.

5. **Auth is thin and the docs admit it.** `Util.hash` is salted SHA-256 — a
   FAST hash. The security docs themselves say it's only "fine for internal
   tools" and recommend a slow KDF for real users. My clients ask for "staff
   login, add users" and there's no roles, no OAuth, no admin scaffold. I'd
   have to build all of it. In PHP I'd at least have a package or an existing
   pattern.

6. **Deployment is the wall.** The single binary is great ON a VPS. But
   most of my existing clients are on cPanel shared hosting where I don't
   have a shell. Bialet speaks HTTP/1.0 and has no native TLS — you need a
   reverse proxy and root. That rules out exactly the hosting my business
   runs on. And my dev machine is Windows: the cross-compiled binary needs
   three DLLs shipped beside it. Every other framework I use just installs.

7. **Migrations are name-based with no rollback.** `Db.migrate("name", sql)`
   tracks by name. There's no `down()`, no version numbers, no way to undo
   last week's schema change when a client asks. For a one-man project,
   fine. I've had clients ask to "revert the change" — I can't with this.

8. **The long-term question.** Wren is a niche dialect of a niche language.
   When I'm 50 and a client needs this maintained, who takes over? There's no
   talent pool, no Stack Overflow mass, no ecosystem. That's not a knock on
   the code — it's a rational business objection and it's the hardest thing
   to overcome.

## What I loved

- The manifesto is written by someone who feels my pain. "Ride Light" isn't
  marketing, it's the actual design.
- File-based routing + query strings is exactly `$_GET`, and I prefer it to
  Laravel's router for these projects.
- Raw SQL. No ORM lying to me.
- One binary to deploy to a VPS with systemd. I'll evangelize that to other
  freelancers who own a box.

## Scorecard (PHP veteran eyes)

| Aspect | Grade | Note |
|---|---|---|
| File-based routing | A | the PHP model, done right |
| Prepared statements / SQL | A | stricter than PDO, good |
| CSRF / sessions | F | multi-form breakage, no-PK table, undefined read order |
| Auth story | D | fast-hash default, no roles/OAuth/admin |
| Deployment | D | VPS-only; no shared-hosting path; no TLS |
| Windows story | D | DLL juggling |
| Migrations | C | no rollback, no versions |
| Long-term maintainability | D | niche language, no talent pool |
| The manifesto | A | speaks my language |

## Concrete asks

1. **Fix `BIALET_SESSION`**: add a primary key on `(id, key)` so `REPLACE`
   replaces, and make `get()` order deterministically (`ORDER BY updatedAt
   DESC LIMIT 1`). This one fix removes the CSRF flakiness AND the unbounded
   session growth.
2. **Document the multi-form CSRF trap** and the single-token workaround, or
   make `csrf` not rotate the stored token on every render.
3. **A shared-hosting story**, or be explicit that a VPS is required. Right
   now the marketing implies any old host works.
4. **Native TLS + HTTP/1.1** so a domain can point straight at the binary.
5. **Versioned migrations with rollback** (even a down-script convention).
6. **A self-contained Windows build** (`make static` does it for Linux;
   do the same for Windows so there are no DLLs).
7. **An admin scaffold** — a generated CRUD admin over a table would cover
   most of my client work.
