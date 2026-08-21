# HTML-First

Bialet renders on every request. There's no client-side router, no
hydration step, no state to keep in sync between server and browser. That
model works best when the browser does what the browser already knows how
to do, and JavaScript is reserved for the few things it genuinely can't.

This isn't a new idea — see
[You Might Not Need JS](http://youmightnotneedjs.com/) and
[HTML Can Do That](https://chrisburnell.com/html-can-do-that/) for the
wider case. This page is the Bialet-flavored version: which native
HTML/CSS features replace common JS patterns, and how they look in a
`.wren` file.

## Why this fits Bialet specifically

Bialet has no bundler and no package manager. Every script tag you add is
a file you wrote, by hand, that you now maintain forever — there's no
`npm update` to carry it forward. That raises the bar for reaching for JS
in the first place: a checkbox hack that ships zero bytes of script beats
a hand-rolled toggle function you'll be debugging in a year.

It also means the trade Bialet already made — render HTML on the server
per request instead of shipping state to the client — pairs naturally
with letting the *server* own things like sorting, filtering, and
pagination via plain links and query params, instead of re-implementing
them in JS on top of a JSON endpoint.

None of this is a purity rule. If a feature genuinely needs JS —
drag-and-drop, live validation against the server, anything real-time —
write the JS. The point is to check whether HTML already does it first.

## Disclosure: `<details>` / `<summary>`

FAQ sections, "read more" blocks, and dropdown menus are a disclosure
widget. The browser already has one:

```wren
return <main>
  <h1>FAQ</h1>
  <details>
    <summary>Do I need a database?</summary>
    <p>No — Bialet works without one, but SQLite is built in if you do.</p>
  </details>
  <details>
    <summary>Can I still use JavaScript?</summary>
    <p>Yes. Bialet doesn't stop you — it just doesn't make you start there.</p>
  </details>
</main>
```

Style the `<summary>` marker and open/closed state with the `::marker`
pseudo-element and the `[open]` attribute selector — no click handler
required.

## Popovers and menus: the `popover` attribute

Tooltips, dropdown menus, and lightweight modals used to be the textbook
"you need JS for this" example. The `popover` attribute and
`popovertarget` make the browser own the show/hide/light-dismiss/`Esc`
behavior:

```wren
return <nav>
  <button popovertarget="user-menu">Account</button>
  <div popover id="user-menu">
    <a href="/profile">Profile</a>
    <a href="/logout">Log out</a>
  </div>
</nav>
```

Clicking outside, pressing `Esc`, or clicking another `popovertarget`
closes it automatically. Check current browser support before relying on
it for a feature that must work everywhere.

## Tabs and toggles: the checkbox/radio hack

Radio buttons already track "which one is selected" — that's a tab strip.
Pair hidden inputs with CSS `:checked` and a sibling or `:has()` selector
instead of a JS tab controller:

```wren
return <div class="tabs">
  <input type="radio" name="tab" id="tab-1" checked>
  <input type="radio" name="tab" id="tab-2">
  <div class="tab-labels">
    <label for="tab-1">Overview</label>
    <label for="tab-2">Settings</label>
  </div>
  <section class="panel-1">Overview content</section>
  <section class="panel-2">Settings content</section>
</div>
```

```css
.panel-1, .panel-2 { display: none; }
#tab-1:checked ~ .panel-1, #tab-2:checked ~ .panel-2 { display: block; }
```

The same hack covers mobile nav toggles, dark-mode switches (without
persistence — that part needs a cookie or a tiny script), and
show/hide password fields via `<input type="checkbox">` and `:has()`.

## Closing dialogs without JS

Opening a `<dialog>` as a true modal (`.showModal()`) still needs one line
of JS — that's an honest limit, not a workaround to hide. But *closing*
it doesn't:

```wren
return <dialog id="confirm">
  <form method="dialog">
    <p>Delete this post?</p>
    <button value="cancel">Cancel</button>
    <button value="confirm">Delete</button>
  </form>
</dialog>
```

`<form method="dialog">` closes the nearest `<dialog>` and sets
`returnValue` to the button that submitted — no `close()` call needed on
the way out.

## Native form validation

`required`, `pattern`, `minlength`, `maxlength`, `min`, `max`, and typed
inputs (`type="email"`, `type="url"`, `type="number"`) validate in the
browser before the request is even sent, and `:invalid` / `:user-invalid`
in CSS style the bad state:

```wren
return <form method="post">
  <label>Email
    <input name="email" type="email" required>
  </label>
  <label>Age
    <input name="age" type="number" min="13" max="120" required>
  </label>
  <button>Sign up</button>
</form>
```

This is client-side convenience only — always re-validate on the server,
the same way you already guard `Request.post()` against `null`. See
[Forms](forms.md) for the server-side half of this pattern.

## Autocomplete: `<datalist>`

An autocomplete field backed by your database doesn't need a JS
keystroke handler and a fetch call — render the options server-side into
a `<datalist>`:

```wren
var tags = `SELECT DISTINCT tag FROM posts`.fetch

return <form method="get">
  <input name="tag" list="tag-options">
  <datalist id="tag-options">
    {{ tags.map { |t| <option value="{{ t["tag"] }}"></option> } }}
  </datalist>
  <button>Filter</button>
</form>
```

## Sorting, filtering, and pagination: let the server do it

This is the pattern Bialet is built for. Instead of shipping a sort
function to the client, make sorting a link that re-requests the page
with a different query param — the server already re-renders on every
request, so there's no extra round trip to justify a JS version:

```wren
var sort = Request.get("sort") || "created_at"
var allowed = ["created_at", "title", "views"]
if (!allowed.contains(sort)) sort = "created_at"

var posts = `SELECT * FROM posts ORDER BY {{ sort }} DESC`.fetch

return <main>
  <nav>
    <a href="?sort=created_at">Newest</a>
    <a href="?sort=title">Title</a>
    <a href="?sort=views">Most viewed</a>
  </nav>
  <ul>
    {{ posts.map { |p| <li>{{ p["title"] }}</li> } }}
  </ul>
</main>
```

Note the sort column is checked against an allow-list before reaching the
SQL string — interpolating a raw query param into `ORDER BY` is a SQL
injection hole, allow-list or reject it. See [Security](security.md).

Pagination and filtering follow the same shape: read query params, adjust
the SQL, render links to the next state. It's slower to click through
than a client-side re-sort, but it works with JS off, needs no state
management, and is indexable by search engines.

## When to actually reach for JS

HTML and CSS run out for anything that needs to react to *time* or to
the *server* without a full page load: live search-as-you-type,
drag-and-drop reordering, optimistic UI, WebSocket-driven updates. Write
the JS for those — a single small `<script>` tag, not a framework. The
goal isn't zero JavaScript; it's not paying for it where the browser
already gives it to you for free.
