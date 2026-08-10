// Migration — runs automatically on server start AND when any .wren file changes.
// No migration CLI, no versioned .sql files, just a named Db.migrate() call. Wild.
//
// Big gotcha I already learned from the docs: DB column values come back as
// STRINGS. So `finished` will be "0"/"1" here, not a real boolean. 🙃

Db.migrate("Create tasks table", `
  CREATE TABLE tasks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    description TEXT NOT NULL,
    finished BOOLEAN DEFAULT 0,
    session TEXT,
    createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
  )
`)
