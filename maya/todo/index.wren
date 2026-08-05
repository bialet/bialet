// ================= MY VERY FIRST BACKEND APP!!! =================
// A todo list. No npm install. No node_modules. No database server.
// Just this file and a little SQL that I am still learning. 😅
//
// Things I learned the hard way (so future-Maya doesn't cry):
//   1. Request.post("x") returns null when the field is missing.
//      Calling .trim() on null = blank 500 error page. Use || "".
//   2. "" is TRUTHY in Wren. So `if (text)` does NOT work.
//      You need `if (text != "")`. I lost 20 minutes to this.
//   3. The database gives me STRINGS back. Even numbers.
//      COUNT(*) came back as "3" not 3. .toNum fixes it.
//   4. I forgot .safe on the task text at first. The security page
//      yelled at me (XSS!!). Now I put .safe on every user string.

// The README said the table just creates itself. Magic. ✨
`CREATE TABLE IF NOT EXISTS todos (
  id   INTEGER PRIMARY KEY AUTOINCREMENT,
  text TEXT    NOT NULL,
  done INTEGER DEFAULT 0
)`.query

// ------------------ handle form submits ------------------
// All three actions (add / toggle / delete) post back to "/"
// with a hidden "action" field. One file does everything.
if (Request.isPost) {
  var action = Request.post("action") || ""
  var id     = Request.post("id") || ""

  if (action == "add") {
    var text = Request.post("text") || ""
    if (text != "") {
      // parameterized query, placeholders = safe
      `INSERT INTO todos (text) VALUES (?)`.query(text)
    }

  } else if (action == "toggle") {
    if (id != "") {
      // flips 0 <-> 1 in pure SQL. felt clever.
      `UPDATE todos SET done = 1 - done WHERE id = ?`.query(id)
    }

  } else if (action == "delete") {
    if (id != "") {
      `DELETE FROM todos WHERE id = ?`.query(id)
    }
  }

  // always redirect after POST so refresh doesn't re-submit
  return Response.redirect("/")
}

// ------------------ load the data ------------------
var todos = `SELECT * FROM todos ORDER BY id DESC`.fetch
var remaining = `SELECT COUNT(*) FROM todos WHERE done = 0`.toNum

// ------------------ the view ------------------
// NOTE: I FIRST wrote <div> around a list of <div> cards and the
// parser said NO — "the outermost tag cannot repeat". In HTML that
// is 100% legal. Wrapping everything in <main> made it happy.
return <main>
  <h1>My Todo List</h1>
  <p class="remaining">{{ remaining }} thing{{ remaining != 1 && "s" }} left to do</p>

  <form method="post" class="add-form">
    <input type="hidden" name="action" value="add" />
    <input type="text" name="text" placeholder="What do you need to do?" autofocus />
    <button type="submit">Add</button>
  </form>

  {{ todos.count == 0 && <p class="empty">Nothing here yet. Add your first task!</p> }}

  <ul>
    {{ todos.map { |t| <li class="{{ t["done"] == "1" && "done" }}">
      <form method="post" class="mini">
        <input type="hidden" name="action" value="toggle" />
        <input type="hidden" name="id" value="{{ t["id"] }}" />
        <button type="submit" class="toggle">{{ t["done"] == "1" ? "↩" : "✓" }}</button>
      </form>
      <span class="text">{{ t["text"].safe }}</span>
      <form method="post" class="mini">
        <input type="hidden" name="action" value="delete" />
        <input type="hidden" name="id" value="{{ t["id"] }}" />
        <button type="submit" class="delete">✕</button>
      </form>
    </li> } }}
  </ul>

  <p class="footer">built with bialet · zero packages installed · i can't believe it</p>
</main>
