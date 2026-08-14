class Task {
  construct new(task) {
    _id = task["id"]
    _description = task["description"] || ""
    _finished = task["finished"] || false
    _session = task["session"] || Session.id
    _createdAt = task["createdAt"] || Date.new()
  }
  static new() { Task.new({}) }

  id { _id }
  finished { _finished == "1" || _finished == true }
  description { _description.trim() != "" ? _description : "No description" }
  createdAt { Date.new(_createdAt) }
  description=(val) { _description = val.toString.trim() }

  save() { _id = `Task`.save(this) }

  toString { "id:%( id ) | %( description )" }

  static toggle(id) { `UPDATE Task SET finished = ((finished | 1) - (finished & 1))
    WHERE id = ? AND session = ?`.query(id, Session.id) }

  static list(finished) { `SELECT * FROM Task
    WHERE session = ? AND (? = 1 OR finished = ?)
    ORDER BY createdAt ASC`.fetch(Session.id, finished == "all", finished == "active").to(Task) }

  static countActive() { `SELECT COUNT(*) as cnt FROM Task
    WHERE session = ? AND finished = 0`.toNum(Session.id) }

  static countCompleted() { `SELECT COUNT(*) as cnt FROM Task
    WHERE session = ? AND finished = 1`.toNum(Session.id) }

  static clear() { `DELETE FROM Task
    WHERE finished = 1 AND session = ?`.query(Session.id) }

  static delete(id) { `DELETE FROM Task
    WHERE id = ? AND session = ?`.query(id, Session.id) }

  static clearAll() { `DELETE FROM Task WHERE finished = 1`.query }
}
