// Domain model. Explicit parameterized SQL on purpose.
// The reflection-based `table`.save(obj) exists but I don't trust it:
// it would stringify a Bool as "true"/"false" into an INTEGER column.
// 20 years of PHP taught me to write the INSERT myself.
//
// FOOTGUN (stock Wren, undocumented in Bialet docs): a method body is an
// implicit-return "expression body" ONLY when the expression starts on the
// SAME line as the "{". Put a newline after "{" and it becomes a statement
// body that silently returns null -- no warning, your model just returns
// empty. That burned me for a full hour (list() returned 0 rows that were
// definitely in the table). Fix: always write `return` explicitly.

class Task {
  construct new(row) {
    _id = row["id"]
    _title = row["title"] || ""
    // SQLite returns every column as a string. Compare as a string.
    _done = row["done"] == "1"
  }

  id { _id }
  title { _title }
  done { _done }

  static create(title) {
    return `INSERT INTO tasks (title) VALUES (?)`.query(title)
  }

  static list() {
    return `SELECT * FROM tasks ORDER BY id`.fetch.to(Task)
  }

  static find(id) {
    // .to(Task) on a null first() returns null -- Null.to is a no-op.
    return `SELECT * FROM tasks WHERE id = ?`.first(id).to(Task)
  }

  toggle() {
    return `UPDATE tasks SET done = CASE WHEN done = 0 THEN 1 ELSE 0 END WHERE id = ?`.query(_id)
  }

  static delete(id) {
    return `DELETE FROM tasks WHERE id = ?`.query(id)
  }
}
