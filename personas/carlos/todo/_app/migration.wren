// Schema. Ran automatically at startup.
// No separate migration runner, no dump files to babysit.
// This is like a PHP migration file minus the version framework.
Db.migrate("create tasks table", `
  CREATE TABLE IF NOT EXISTS tasks (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    text       TEXT    NOT NULL,
    done       INTEGER NOT NULL DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
  )`)
