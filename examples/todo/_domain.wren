import "bialet" for Db, Session

class Task {

  construct new() { _session = Session.new().id }

  list() {
    var tasks = Db.all("
      SELECT * FROM tasks
      WHERE session = ?
      ORDER BY createdAt ASC
    ", [_session])
    tasks.map{ normalize }
    return tasks
  }

  save(description) {
    Db.save("tasks", {
      "description": description.trim(),
      "finished": false,
      "session": _session
    })
  }

  toggleFinished(id) {
    Db.query("UPDATE tasks
        SET finished = ((finished | 1) - (finished & 1))
        WHERE id = ? AND session = ?", [id, _session])
  }

  clearFinished() { Db.query("
    DELETE FROM tasks
    WHERE finished = 1 AND session = ?
  ", [_session]) }

  normalize(task) {
    task["description"] = task["description"].trim()
    if (task["description"] == "") {
      task["description"] = "No description"
    }
    return task
  }
}
