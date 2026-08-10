// Elena's "React components" — static methods that return HTML strings.
// No props object, no children, no keys. You pass args. It composes fine.
// The filter is done with Alpine `x-show` on each row + a query param for the
// initial state, because that's exactly how I'd prototype it in React/Vue land.

class TaskItem {
  // Each row carries its own `done` flag in Alpine scope, and reads `filter`
  // from the parent container's x-data. Child scope reads parent scope — nice.
  static render(task, csrf) { <li class="group flex items-center gap-3 rounded-lg px-2 py-2 hover:bg-gray-50"
      x-data="{ done: {{ task.finished ? "true" : "false" }} }"
      x-show="filter == 'all' || (filter == 'open' && !done) || (filter == 'done' && done)">
    <form method="post" action="/toggle" class="flex items-center">
      {{ csrf }}
      <input type="hidden" name="id" value="{{ task.id }}" />
      <button type="submit"
              class="{{ task.finished ? "text-emerald-500" : "text-gray-300 hover:text-gray-500" }}"
              title="{{ task.finished ? "Mark as open" : "Mark as done" }}"
              aria-label="{{ task.finished ? "Mark as open" : "Mark as done" }}">
        {{ task.finished ? "✓" : "○" }}
      </button>
    </form>
    <span class="flex-1 truncate {{ task.finished ? "text-gray-400 line-through" : "text-gray-800" }}">
      {{ task.description }}
    </span>
    <form method="post" action="/delete" class="opacity-0 transition-opacity group-hover:opacity-100">
      {{ csrf }}
      <input type="hidden" name="id" value="{{ task.id }}" />
      <button type="submit" class="text-gray-300 hover:text-red-500" title="Delete task" aria-label="Delete task">✕</button>
    </form>
  </li> }
}

class Filters {
  static render(current) { <nav class="flex gap-1 rounded-xl bg-gray-100 p-1 text-sm">
    <a href="/?filter=all" class="flex-1 rounded-lg px-3 py-1.5 text-center {{ current == "all" ? "bg-white font-semibold text-gray-900 shadow-sm" : "text-gray-500 hover:text-gray-800" }}">All</a>
    <a href="/?filter=open" class="flex-1 rounded-lg px-3 py-1.5 text-center {{ current == "open" ? "bg-white font-semibold text-gray-900 shadow-sm" : "text-gray-500 hover:text-gray-800" }}">Open</a>
    <a href="/?filter=done" class="flex-1 rounded-lg px-3 py-1.5 text-center {{ current == "done" ? "bg-white font-semibold text-gray-900 shadow-sm" : "text-gray-500 hover:text-gray-800" }}">Done</a>
  </nav> }
}

class Stats {
  static render(openCount, doneCount, total) { <div class="flex items-center justify-between text-sm text-gray-500">
    <p><strong class="text-gray-800">{{ openCount }}</strong> open</p>
    <p><strong class="text-gray-800">{{ doneCount }}</strong> done · <strong class="text-gray-800">{{ total }}</strong> total</p>
  </div> }
}
