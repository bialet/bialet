// index.wren — the / route. Controller logic at the top, view at the bottom.
// This is the page with THREE forms (add + per-row toggle/delete), so it's also
// where I'm stress-testing multi-form CSRF: every form gets {{ session.csrf }}.
// If the token rotates per call, only the last form's POST will validate.

import "_app/template" for Template
import "_app/domain" for Task
import "_app/components" for TaskItem, Filters, Stats

var session = Session.new()

if (Request.isPost) {
  // CSRF gate first — always. https://media.giphy.com/media/xT0xeJpnrWC4XWblEk/giphy.gif
  if (!session.csrfOk) {
    Response.status(400)
    return Template.new().layout(<main class="mx-auto max-w-2xl px-4 py-10">
      <h1 class="text-2xl font-bold text-gray-900">Bad CSRF token 🙅</h1>
      <p class="mt-2 text-gray-600">Your form submission was rejected. Go back and try again.</p>
    </main>)
  }
  var task = Task.new()
  task.description = Request.post("description") || "" // null-safe, the docs shouted about this
  task.save()
  return Response.redirect("/") // Post/Redirect/Get — no duplicate adds on refresh
}

// ?filter=all|open|done sets the Alpine initial state; Alpine does the rest.
var filter = Request.get("filter") || "all"
if (filter != "all" && filter != "open" && filter != "done") {
  filter = "all"
}

var tasks = Task.list()
var openCount = tasks.where { |t| !t.finished }.count
var doneCount = tasks.count - openCount

return Template.new().layout(<main class="mx-auto max-w-2xl px-4 py-10">
  <header class="mb-8">
    <h1 class="text-3xl font-bold text-gray-900">Elena's Todo</h1>
    <p class="mt-1 text-sm text-gray-500">React muscle memory, Bialet engine.</p>
  </header>

  <section class="rounded-2xl bg-white p-5 shadow-sm">
    <form method="post" class="flex gap-2">
      {{ session.csrf }}
      <input name="description" placeholder="What needs doing?" required autofocus
             class="flex-1 rounded-lg border border-gray-200 px-3 py-2 text-gray-800 outline-none focus:border-emerald-400" />
      <button type="submit" class="rounded-lg bg-emerald-500 px-4 py-2 font-medium text-white hover:bg-emerald-600">Add</button>
    </form>
  </section>

  <div class="mt-6 space-y-4" x-data="{ filter: '{{ filter }}' }">
    {{ Filters.render(filter) }}
    {{ Stats.render(openCount, doneCount, tasks.count) }}
    <ul class="divide-y divide-gray-100 rounded-2xl bg-white p-2 shadow-sm">
      {{ tasks.map{ |t| TaskItem.render(t, session.csrf) } }}
    </ul>
    {{ tasks.count == 0 && <p class="py-10 text-center text-gray-400">Nothing here yet. Add a task above ✨</p> }}
  </div>
</main>)
