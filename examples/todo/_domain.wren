import "bialet" for Db, Session

class Task {

  construct new() { _session = Session.new().id }

  list() { `
      SELECT * FROM tasks
      WHERE session = ?
      ORDER BY createdAt ASC
    `.set(_session).map{ normalize }
  }

  save(description) {
    `INSERT INTO tasks (description, finished, session) VALUES (?, ?, ?)`.set(description.trim(), false, _session).lastInsertedId
  }

  toggleFinished(id) { `
    UPDATE tasks
        SET finished = ((finished | 1) - (finished & 1))
        WHERE id = ? AND session = ?`.call(id, _session)
  }

  clearFinished() { `
    DELETE FROM tasks
    WHERE finished = 1 AND session = ?
  `.call(_session) }

  normalize(task) {
    task["description"] = task["description"].trim()
    if (task["description"] == "") {
      task["description"] = "No description"
    }
    return task
  }
}
