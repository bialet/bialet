import "bialet" for Db

Db.migrate("Tasks Table", `
  CREATE TABLE Task (
    id INTEGER PRIMARY KEY,
    description TEXT,
    finished INTEGER,
    session TEXT,
    createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
  )
`)
