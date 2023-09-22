class Task {
  normalizedDescription(task) {
    task["description"] = task["description"].trim()
    if (task["description"] == "") {
      task["description"] = "No description"
    }
    return task
  }
}
