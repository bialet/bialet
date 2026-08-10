// =============================================================================
// MY FIRST SERVER-SIDE APP EVER!! a todo list 🎉
//
// one single file. no node_modules, no package.json, no bundler, no YAML
// maze. just a .wren file and a css file and a bicycle. exactly what I signed
// up for when I read "no NPM, no YAML, no separate database servers."
// =============================================================================

// -----------------------------------------------------------------------------
// 1. DATABASE
// the getting-started tutorial puts tables in a _migration.wren file, but I
// decided to keep EVERYTHING in this one file, and the README shows a trick:
// run CREATE TABLE IF NOT EXISTS at the top of a page. it re-runs on every
// request but is a no-op after the first time, so it's free. the _db.sqlite3
// file just appears next to my code. zero setup. it's honestly a little
// magical and I love it.
// -----------------------------------------------------------------------------
`CREATE TABLE IF NOT EXISTS todos (
  id   INTEGER PRIMARY KEY AUTOINCREMENT,
  text TEXT NOT NULL,
  done INTEGER NOT NULL DEFAULT 0
)`.query

// -----------------------------------------------------------------------------
// 2. HANDLE FORM POSTS
// every form on the page posts to "/" and tells me what it wants with a hidden
// "action" field. the docs drilled this into me: check -> process -> redirect.
// NEVER forget the `return` before Response.redirect. (I forgot. twice. the
// docs promise a "double-response error" but 0.12.0 just sends a 302 with my
// page body glued on and no log line. it "worked", silently. see NOTES.md)
// -----------------------------------------------------------------------------
if (Request.isPost) {
  var action = Request.post("action") || ""

  if (action == "add") {
    // Request.post() returns null when the field is missing, and null.trim()
    // would crash the whole page. the docs screamed about this and they were
    // right. `|| ""` is my shield.
    var text = Request.post("text") || ""
    `INSERT INTO todos (text) VALUES (?)`.query(text)
    return Response.redirect("/")
  }

  if (action == "toggle") {
    var id = Request.post("id") || ""
    var task = `SELECT * FROM todos WHERE id = ?`.first(id)
    // LEARNED THE HARD WAY: sqlite hands you STRINGS, not numbers.
    // so "done" is "0" or "1", and I compare against the string.
    var newDone = task["done"] == "0" ? 1 : 0
    `UPDATE todos SET done = ? WHERE id = ?`.query(newDone, id)
    return Response.redirect("/")
  }

  if (action == "delete") {
    var id = Request.post("id") || ""
    `DELETE FROM todos WHERE id = ?`.query(id)
    return Response.redirect("/")
  }
}

// -----------------------------------------------------------------------------
// 3. LOAD THE DATA (also strings, also cursed)
// -----------------------------------------------------------------------------
var todos = `SELECT * FROM todos ORDER BY id DESC`.fetch
var remaining = `SELECT COUNT(*) FROM todos WHERE done = 0`.toNum

// -----------------------------------------------------------------------------
// 4. THE VIEW
// <main> is the outer tag so I can nest <div>s freely inside it.
// (see the "Cannot nest <div> inside <div>" saga in NOTES.md — the outer tag
// can never repeat inside an inline HTML string. took me ~10 minutes and a
// lot of sad staring to figure that out.)
//
// security note that just works: {{ t["text"] }} escapes automatically. I
// pasted "<script>alert(1)</script>" in as a task on purpose and the page
// showed it as literal text. so I can't shoot myself in the foot by accident.
// -----------------------------------------------------------------------------
return <main class="app">
  <header>
    <h1>things to do</h1>
    <p class="counter">{{ remaining }} left</p>
  </header>

  <form method="post" action="/" class="add-form">
    <input type="hidden" name="action" value="add" />
    <input type="text" name="text" placeholder="what needs doing?" required autofocus />
    <button type="submit">add</button>
  </form>

  {{ todos.count == 0 && <p class="empty">
    nothing here yet — add your first task above
  </p> }}

  <ul class="todo-list">
    {{ todos.map { |t| <li class="todo-item">
      <form method="post" action="/" class="toggle-form">
        <input type="hidden" name="action" value="toggle" />
        <input type="hidden" name="id" value="{{ t["id"] }}" />
        <button type="submit" class="toggle {{ t["done"] == "1" && "checked" }}"
                aria-label="toggle task">{{ t["done"] == "1" ? "✓" : "○" }}</button>
      </form>
      <span class="todo-text {{ t["done"] == "1" ? "done" : "" }}">{{ t["text"] }}</span>
      <form method="post" action="/" class="delete-form">
        <input type="hidden" name="action" value="delete" />
        <input type="hidden" name="id" value="{{ t["id"] }}" />
        <button type="submit" class="delete" aria-label="delete task">✕</button>
      </form>
    </li> } }}
  </ul>
</main>