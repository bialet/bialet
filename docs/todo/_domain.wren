import "bialet" for Session, Db

class Task {

  // Constructors
  construct new() {
    _id = null
    _description = ""
    _finished = false
    _session = Session.new().id
  }
  construct new(id) { load_(`SELECT * FROM tasks WHERE id = ?`.first(id)) }
  construct from(task) { load_(task) }

  // Getters and Setters
  id { _id }
  description { _description.trim() != "" ? _description : "No description" }
  finished { _finished }
  description=(val) { _description = val.toString.trim() }

  // New syntax!
  // load_(task) { _** = task }
  // save() { Db.save('tasks', _**) }

  // Helper method to load a task
  load_(task) {
    _id = task["id"]
    _description = task["description"]
    _finished = task["finished"]
    _session = task["session"]
  }

  // Save a task into the database
  save() { Db.save('tasks', {
    "id": _id,
    "description": _description,
    "finished": _finished,
    "session": _session,
  }) }

  // Toggle a task
  toggleFinished() {
      `UPDATE tasks
      SET finished = ((finished | 1) - (finished & 1))
      WHERE id = ? AND session = ?`.query(_id, _session)
      _finished = !_finished
    }

  // Static methods

  // List all tasks for the current session
  static list() { `
    SELECT * FROM tasks WHERE session = ? ORDER BY createdAt ASC
  `.fetch(Session.new().id).map{ |task| Task.from(task) } }

  // Clear finished tasks for the current session
  static clearFinished() { `
    DELETE FROM tasks WHERE finished = 1 AND session = ?
    `.query(Session.new().id) }

  // Clear all finished tasks
  static clearAll() { `DELETE FROM tasks WHERE finished = 1`.query }
}
