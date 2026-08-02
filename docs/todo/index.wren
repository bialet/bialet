import "_template" for Template
import "_domain" for Task

if (Request.isPost) {
  var task = Task.new()
  task.description = Request.post("task") || ""
  task.save()
  System.log("New task created: %(task)")
  return Response.redirect("/")
}

var filter = Request.get("filter") || "all"
var tasks = (filter == "active") ? Task.listActive() : (filter == "completed") ? Task.listCompleted() : Task.list()
var activeCount = Task.countActive()
var completedCount = Task.countCompleted()


var showClear = (tasks.count > 0 && completedCount > 0)

var emptyMessage
if (filter == "active") {
  emptyMessage = "No active tasks. Enjoy your free time!"
} else if (filter == "completed") {
  emptyMessage = "Nothing completed yet. Keep going!"
} else {
  emptyMessage = "Your list is empty. Add your first task above."
}

return Template.new().layout(<main>

  <form method="post" class="create-form">
    <section class="input-group">
      <input name="task" placeholder="What needs to be done?" required autofocus />
      <button type="submit">Add task</button>
    </section>
  </form>

  <nav class="filters" aria-label="Task filters">
    <a href="/" class="filter-tab {{ filter == "all" && "active" }}">All</a>
    <a href="/?filter=active" class="filter-tab {{ filter == "active" && "active" }}">Active</a>
    <a href="/?filter=completed" class="filter-tab {{ filter == "completed" && "active" }}">Completed</a>
  </nav>

  <aside class="stats-bar">
    <span class="stats-count"><strong>{{ activeCount }}</strong> task{{ activeCount != 1 && "s" }} remaining</span>
    {{ showClear && <form method="post" action="/clear" class="clear-form"><button class="clear-btn">Clear completed</button></form> }}
  </aside>

  <ul class="task-list">
    {{ tasks.map{ |task| <li class="task-item {{ task.finished && "completed" }}">
        <section class="task-item-row">
          <form method="post" action="/toggle" class="checkbox-form">
            <input type="hidden" name="id" value="{{ task.id }}" />
            <button class="{{ task.finished ? "checkbox-btn checked" : "checkbox-btn" }}" title="{{ task.finished ? "Mark incomplete" : "Mark complete" }}" aria-label="{{ task.finished ? "Mark incomplete" : "Mark complete" }}"></button>
          </form>
          <span class="task-content">
            <span class="{{ task.finished ? "task-text completed" : "task-text" }}">
              <span class="priority-dot {{ Num.fromString(task.id.toString) % 3 == 1 ? "high" : Num.fromString(task.id.toString) % 3 == 2 ? "medium" : "low" }}" aria-hidden="true"></span>
              {{ task.description.safe }}
            </span>
            <span class="task-meta">{{ task.createdAt.hh }}:{{ task.createdAt.mi }}</span>
          </span>
          <form method="post" action="/delete" class="delete-form">
            <input type="hidden" name="id" value="{{ task.id }}" />
            <button class="delete-btn" title="Delete task" aria-label="Delete task">✕</button>
          </form>
        </section>
      </li> } }}
  </ul>
  {{ tasks.count == 0 && <section class="empty-state">
    <span class="empty-icon" aria-hidden="true">🎉</span>
    <p class="empty-text">{{ emptyMessage }}</p>
  </section> }}

</main>)
