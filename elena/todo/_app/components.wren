// "Components". In React this would be a .tsx function. Here it's a
// Wren static method returning an HTML string. Close enough to JSX that
// it tricks you, then the parser reminds you it isn't.
//
// THINGS THE PARSER REJECTED, that JSX allows:
//  - <div> wrapping <div> cards — "outermost tag cannot repeat". This is
//    the single most common layout pattern on earth. I flattened to <li>.
//  - Multi-line logic inside {{ }} — the expression must start on the
//    same line as {{. I kept forgetting and got silent empty output.
//  - A <custom-tag> name — hyphens are illegal in tag names. Web
//    components? Nope.
//
// And it does NOT auto-escape. React escapes by default. Here forgetting
// one `.safe` is an XSS hole. My React muscle memory wrote vulnerable
// code for an hour before the security page set me straight.

class TaskItem {
  static render(task, csrf) {
    <li class="flex items-center gap-3 bg-white rounded-xl shadow-sm px-4 py-3 {{ task["done"] == "1" && "opacity-60" }}"
        x-show="filter === 'all' || filter === '{{ task["done"] == "1" ? "done" : "open" }}'">
      <form method="post" action="/toggle" class="inline">
        {{ csrf }}
        <input type="hidden" name="id" value="{{ task["id"] }}" />
        <button type="submit"
                class="w-6 h-6 rounded-full border-2 shrink-0 {{ task["done"] == "1" ? "border-indigo-600 bg-indigo-600" : "border-slate-300 hover:border-indigo-400" }}"></button>
      </form>
      <span class="flex-1 text-slate-700 {{ task["done"] == "1" && "line-through" }}">{{ task["text"].safe }}</span>
      <form method="post" action="/delete" class="inline">
        {{ csrf }}
        <input type="hidden" name="id" value="{{ task["id"] }}" />
        <button type="submit" class="text-slate-400 hover:text-red-500 text-sm">✕</button>
      </form>
    </li>
  }
}
