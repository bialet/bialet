// Main page. Controller on top, view below — Bialet's "MVC in one file".
// The routing is just the filesystem: this file IS "/".
import "_app/template" for Template
import "_app/components" for TaskItem
import "_app/domain" for Task

var session = Session.new()

// CSRF: {{ session.csrf }} in EVERY form on the page is broken — each
// call writes a NEW token row and get() returns "some" row (the table has
// no primary key, so REPLACE INTO appends forever). Only the last-rendered
// token ever validates. I generate ONE token and reuse it in every form.
// (There is no official multi-form recipe. I reverse-engineered this one.)
var csrf = ""
if (!Request.isPost) {
  csrf = session.csrf
}

// POST — add a task, redirect (Post/Redirect/Get).
if (Request.isPost) {
  if (session.csrfOk) {
    var text = Request.post("text") || ""
    if (text != "") Task.add(text)
  }
  return Response.redirect("/")
}

var tasks = Task.all()
var remaining = `SELECT COUNT(*) FROM tasks WHERE done = 0`.toNum

return Template.layout(<section x-data="{ filter: 'all' }">
  <div class="flex items-center justify-between mb-6">
    <h2 class="text-xl font-semibold text-slate-800">Tasks</h2>
    <span class="text-sm text-slate-500">{{ remaining }} remaining</span>
  </div>

  <form method="post" class="flex gap-2 mb-6">
    {{ csrf }}
    <input type="text" name="text" placeholder="Add a task..."
           class="flex-1 rounded-lg border border-slate-300 px-3 py-2 focus:outline-none focus:ring-2 focus:ring-indigo-400" />
    <button type="submit"
            class="bg-indigo-600 hover:bg-indigo-700 text-white rounded-lg px-4 py-2 text-sm">Add</button>
  </form>

  <div class="flex gap-2 mb-4 text-sm">
    <button @click="filter = 'all'" :class="filter === 'all' ? 'bg-slate-800 text-white' : 'bg-white text-slate-600'"
            class="rounded-full px-3 py-1">All</button>
    <button @click="filter = 'open'" :class="filter === 'open' ? 'bg-slate-800 text-white' : 'bg-white text-slate-600'"
            class="rounded-full px-3 py-1">Open</button>
    <button @click="filter = 'done'" :class="filter === 'done' ? 'bg-slate-800 text-white' : 'bg-white text-slate-600'"
            class="rounded-full px-3 py-1">Done</button>
  </div>

  {{ tasks.count == 0 && <p class="text-slate-400 text-center py-10">No tasks yet. Add your first one above.</p> }}

  <ul class="space-y-2">
    {{ tasks.map { |t| TaskItem.render(t, csrf) } }}
  </ul>
</section>)
