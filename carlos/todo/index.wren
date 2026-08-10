import "_app/domain" for Task
import "_app/template" for Template

// === Controller ===
// One session per request. csrf getter is idempotent within the instance,
// so every {{ session.csrf }} on this page renders the same token.
var session = Session.new()
var error = null

if (Request.isPost) {
  if (!session.csrfOk) {
    // No token / wrong token. Do NOT touch state.
    error = "CSRF token missing or invalid."
  } else {
    // Request.post returns null when the field is absent. Guard it.
    var title = (Request.post("title") || "").trim()
    if (title == "") {
      error = "Task text can't be empty."
    } else {
      Task.create(title)
      // Post/Redirect/Get -- no resubmission on refresh.
      return Response.redirect("/")
    }
  }
}

var tasks = Task.list()

// === View ===
return Template.new().layout(<main>
  <h1>Todo list</h1>

  <form method="post" action="/">
    {{ session.csrf }}
    <input type="text" name="title" placeholder="What needs doing?" autofocus />
    <button type="submit">Add</button>
  </form>

  {{ error && <p class="error">{{ error }}</p> }}

  <ul class="tasks">
    {{ tasks.map {|t| <li class="{{ t.done && "done" }}">
      <form method="post" action="/toggle" class="inline">
        {{ session.csrf }}
        <input type="hidden" name="id" value="{{ t.id }}" />
        <button type="submit" class="toggle">{{ t.done ? "undo" : "done" }}</button>
      </form>
      <span class="title">{{ t.title }}</span>
      <form method="post" action="/delete" class="inline">
        {{ session.csrf }}
        <input type="hidden" name="id" value="{{ t.id }}" />
        <button type="submit" class="delete">delete</button>
      </form>
    </li> } }}
  </ul>

  {{ tasks.count == 0 && <p class="empty">Nothing here yet. Add a task above.</p> }}
</main>)
