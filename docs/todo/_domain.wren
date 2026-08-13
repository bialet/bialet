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

  save() { `Task`.save(this) }

  toggle() {
    `UPDATE Task SET finished = ((finished | 1) - (finished & 1)) WHERE id = ? AND session = ?`.query(_id, Session.id)
    _finished = !finished
    return _finished
  }

  toString { description }

  static list() { `
    SELECT * FROM Task WHERE session = ? ORDER BY createdAt ASC
  `.fetch(Session.id).to(Task) }

  static listActive() { `
    SELECT * FROM Task WHERE session = ? AND finished = 0 ORDER BY createdAt ASC
  `.fetch(Session.id).to(Task) }

  static listCompleted() { `
    SELECT * FROM Task WHERE session = ? AND finished = 1 ORDER BY createdAt ASC
  `.fetch(Session.id).to(Task) }

  static countActive() {
    var r = `SELECT COUNT(*) as cnt FROM Task WHERE session = ? AND finished = 0`.fetch(Session.id)
    return Num.fromString(r[0]["cnt"])
  }

  static countCompleted() {
    var r = `SELECT COUNT(*) as cnt FROM Task WHERE session = ? AND finished = 1`.fetch(Session.id)
    return Num.fromString(r[0]["cnt"])
  }

  static clear() { `
    DELETE FROM Task WHERE finished = 1 AND session = ?
    `.query(Session.id) }

  static delete(id) { `
    DELETE FROM Task WHERE id = ? AND session = ?
    `.query(id, Session.id) }

  static clearAll() { `DELETE FROM Task WHERE finished = 1`.query }
}
