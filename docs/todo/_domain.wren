import "bialet" for Session, Db, Date

class Task {
  construct new(task) {
    _id = task["id"]
    _description = task["description"] || ""
    _finished = task["finished"] || false
    _session = task["session"] || Session.new().id
    _createdAt = task["createdAt"] || Date.new()
  }
  static new() { Task.new({}) }

  id { _id }
  finished { _finished }
  description { _description.trim() != "" ? _description : "No description" }
  createdAt { Date.new(_createdAt) }
  description=(val) { _description = val.toString.trim() }

  save() { Db.save(type, this) }

  toggle() {
    `UPDATE Task SET finished = ((finished | 1) - (finished & 1))
    WHERE id = ? AND session = ?`.query(_id, Session.new().id)
    _finished = `SELECT finished FROM Task WHERE id = ?`.toBool(_id)
  }

  static list() { `
    SELECT * FROM Task WHERE session = ? ORDER BY createdAt ASC
  `.fetch(Session.new().id).map{ |task| Task.new(task) } }

  static clearFinished() { `
    DELETE FROM Task WHERE finished = 1 AND session = ?
    `.query(Session.new().id) }

  static clearAll() { `DELETE FROM Task WHERE finished = 1`.query }
}
