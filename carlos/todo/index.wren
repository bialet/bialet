// Task list + add form.
// The file IS the URL, like index.php. Controller logic on top,
// HTML view below. Familiar shape for someone who wrote PHP for 20 years.
import "_app/template" for Template
import "_app/domain" for Task

var session = Session.new()

// POST/GET CSRF gotcha: putting {{ session.csrf }} in EVERY form on a page
// silently breaks all but the last one — each call rotates the stored token.
// First add task silently "worked" in my tests... then toggle/delete forms
// started failing when I had rows on the page. Generate the token ONCE and
// reuse the same hidden field in every form:
var csrf = ""
if (!Request.isPost) {
  csrf = session.csrf
}

// ----- POST: add a task -----
// CSRF token check, like a token_check() helper in PHP.
if (Request.isPost) {
  if (session.csrfOk) {
    // In PHP I'd write $_POST['text'] ?? ''. Here: Request.post("text") || "".
    var text = Request.post("text") || ""
    if (text != "") Task.add(text)
  }
  return Response.redirect("/")
}

// ----- GET: pick rows by the query string, like $_GET['filter'] -----
var filter = Request.get("filter") || "all"
var tasks
if (filter == "done") {
  tasks = Task.done()
} else if (filter == "open") {
  tasks = Task.open()
} else {
  tasks = Task.all()
}

var open = Task.countOpen()

return Template.layout(<section class="panel">
  <h1>Tasks</h1>
  <p class="muted">{{ open }} open · {{ tasks.count }} shown</p>

  <form method="post" class="add-form">
    {{ csrf }}
    <input type="text" name="text" placeholder="New task..." />
    <button type="submit">Add</button>
  </form>

  {{ tasks.count == 0 && <p class="empty">No tasks in this view.</p> }}

  <ul class="task-list">
    {{ tasks.map { |t| <li class="{{ t["done"] == "1" ? "task done" : "task" }}">
      <form method="post" action="/toggle" class="inline">
        {{ csrf }}
        <input type="hidden" name="id" value="{{ t["id"] }}" />
        <button type="submit" class="link">{{ t["done"] == "1" ? "↩ reopen" : "✓ complete" }}</button>
      </form>
      <span class="text">{{ t["text"].safe }}</span>
      <form method="post" action="/delete" class="inline">
        {{ csrf }}
        <input type="hidden" name="id" value="{{ t["id"] }}" />
        <button type="submit" class="link danger">delete</button>
      </form>
    </li> } }}
  </ul>
</section>)
