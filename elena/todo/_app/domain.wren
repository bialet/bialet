// Elena's Task model. Think of this as my server-side entity class.
// No ORM. No annotations. Just backtick SQL glued together with Wren.
// Honestly? The `?` placeholders feel like prepared statements in better-sqlite3,
// which I can live with.

class Task {
  construct new(data) {
    _id = data["id"]
    _description = data["description"] || ""
    _finished = data["finished"] || false
    _session = data["session"] || Session.id
    _createdAt = data["createdAt"]
  }

  static new() { Task.new({}) }

  id { _id }
  description { _description }
  // SQLite returns strings, so "1" → done. The double-check is me being paranoid.
  finished { _finished == "1" || _finished == true }

  description=(val) { _description = val.toString.trim() }

  // `tasks`.save() auto-INSERTs when there's no id, auto-UPDATEs when there is.
  save() { _id = `tasks`.save(this) }

  toggle() {
    // `RETURNING` would be cute but `.query()` doesn't return it. Just run it.
    `UPDATE tasks SET finished = NOT finished WHERE id = ? AND session = ?`.query(_id, Session.id)
  }

  // Every query is scoped to the session so users only ever see their own tasks.
  static list() { `SELECT * FROM tasks WHERE session = ? ORDER BY id DESC`.fetch(Session.id).to(Task) }
  static delete(id) { `DELETE FROM tasks WHERE id = ? AND session = ?`.query(id, Session.id) }
}
