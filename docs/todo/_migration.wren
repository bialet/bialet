import "bialet" for Db

Db.migrate("Tasks Table", `
  CREATE TABLE tasks (
    id INTEGER PRIMARY KEY,
    description TEXT,
    finished INTEGER,
    session TEXT,
    createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
  )
`)
