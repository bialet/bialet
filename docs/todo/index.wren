import "bialet" for Response, Request, Util
import "_template" for Template
import "_domain" for Task

if (Request.isPost) {
  var task = Task.new()
  task.description = Request.post("task")
  task.save()
  return Response.redirect("/")
}

var tasks = Task.list()
var tasksSection = <section>
  {{ tasks.count > 0 ?
    <ul>
      {{ tasks.map{ |task| <li class="finished_{{ task.finished }}">
          <a
            href="/toggle?id={{ task.id }}"
            title="Created at {{ task.createdAt.format("#H:#M") }} hs">
            {{ Util.htmlEscape(task.description) }}
          </a>
        </li> } }}
    </ul> :
    <p class="no-tasks">No tasks yet</p>
  }}
</section>

var createButton = <form method="post">
  <p>
    <input name="task" placeholder="New task" required />
    <button>Create</button>
  </p>
</form>

var clearButton = <form method="post" action="/clear">
  <p><button>Clear finished tasks</button></p>
</form>

return Template.new().layout(
<main>
  {{ tasksSection }}
  {{ createButton }}
  {{ clearButton }}
</main>
)
