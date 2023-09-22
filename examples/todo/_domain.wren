import "bialet" for Db

class Task {

  construct new() {}

  list() {
    var tasks = Db.all("
      SELECT * FROM tasks
      ORDER BY createdAt ASC
    ")
    tasks.map{ normalize }
    return tasks
  }

  save(description) {
    Db.save("tasks", {
      "description": description.trim(),
      "finished": false
    })
  }

  toggleFinished(id) {
    Db.query("UPDATE tasks
        SET finished = ((finished | 1) - (finished & 1))
        WHERE id = ?", [id])
  }

  clearFinished() { Db.query("
    DELETE FROM tasks
    WHERE finished = 1
  ") }

  normalize(task) {
    task["description"] = task["description"].trim()
    if (task["description"] == "") {
      task["description"] = "No description"
    }
    return task
  }
}
