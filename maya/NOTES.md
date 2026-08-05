# Maya's Session Notes — "I built a backend and nobody can stop me"

First-person log of building `todo/` with Bialet, as Maya would write it.
Honest, slightly terrified, very excited. The app itself is in `todo/`.

## What happened, in order

1. **The install was actually instant.** One command, one binary, and the
   bike logo. I was expecting at least one error message and there wasn't
   one. This part is a 10/10.

2. **The HTML parser told me I don't know HTML.** I wrote `<div>` wrapping a
   bunch of `<div>` cards (a list, the most normal thing on the internet) and
   it refused: "the outermost tag cannot repeat". I stared at my screen for
   five minutes. That rule does not exist in HTML. I had to wrap everything in
   `<main>` to make it work. This is the first thing I'd change if I were
   them.

3. **`Request.post()` tried to kill me.** I forgot the `|| ""` and got a blank
   "Internal Server Error" page with zero hints about which file or line
   broke. I literally guessed by trial and error. Once I read the docs I
   understood: missing form fields are `null`, and calling `.trim()` on
   `null` crashes. Fine. But the error page told me NOTHING.

4. **`""` is truthy.** I wrote `if (text)` to check if the field was empty
   and it always ran. The docs told me to use `if (text != "")`. That is the
   opposite of every other language I know.

5. **The database gives me strings.** Even numbers. `COUNT(*)` came back as
   `"3"`, so my count showed "3" next to the word "things" and I couldn't add
   to it without `.toNum`. I get why (it's a stringly DB layer) but nothing at
   runtime told me — I only found out by reading.

6. **I shipped an XSS hole and didn't know.** I put `{{ task.text }}` in the
   list, tested with `<script>alert(1)</script>`, and the browser would have
   executed it. The docs page yelled at me: "Bialet does not escape by
   default. Use `.safe`." I added `.safe` everywhere. I almost certainly have
   MORE places I missed. For a beginner this default is dangerous.

7. **No CSRF, because I didn't know CSRF was a thing.** I'm a CS student and
   I'd heard the term, but nothing in the README pushed me to add it. My
   guestbook/todo forms are wide open. The security docs cover it, but the
   getting-started path doesn't force you through it. If the framework ships
   CSRF protection, the tutorial should use it.

## What I loved

- The "aha" is real. One file, a form, an INSERT, a SELECT, and it worked on
  `127.0.0.1:7001` with zero packages. No other stack would have let me do
  this in my first hour.
- The single binary. I genuinely did not configure anything.
- The inline HTML, once I knew the rules, is nice. I don't want to learn
  React anymore.

## What almost made me quit

- The generic 500 page. My very first "real" bug (missing `|| ""`) took me
  ~15 minutes to find because the error gave me no file, no line, no hint.
- No VS Code extension. My editor shows my `.wren` files as plain text. I'm
  a student, I live in VS Code. This should exist.
- macOS live-reload. The docs say reload uses Linux-only inotify and I should
  restart the server manually. The "see changes instantly" promise just died
  on my laptop.

## Scorecard (student eyes)

| Aspect | Grade | Note |
|---|---|---|
| Install / zero config | A+ | genuinely magical |
| "Use your HTML skills" | C | parser rejects legal HTML (div-in-div, hyphen tags) |
| Error messages | D | generic 500, no line numbers |
| Beginner safety | D | no auto-escaping, null-crash-by-default |
| Editor support | F | no VS Code extension |
| Live reload | D | Linux-only, manual restart on Mac |
| The aha moment | A | kept it; it's why I'm still here |

## Concrete asks (what would fix my experience)

1. `{{ value }}` escapes by default; `{{ value.raw }}` for trusted markup.
2. A dev-only error page with file + line + a hint ("did you forget `.safe`?
   did you guard the POST field with `|| ""`?").
3. A VS Code extension. Even just syntax highlighting + snippets.
4. Let the HTML parser accept `<div>` inside `<div>` — or give a clear,
   friendly error that suggests the workaround.
5. macOS live reload (kqueue), or stop advertising live reload.
6. `bialet new guestbook` scaffold so my first file isn't blank.
7. Make the getting-started tutorial use CSRF from the start.
