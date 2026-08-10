// Schema. Run once at startup, tracked by name in BIALET_MIGRATIONS.
// Plain CREATE TABLE is fine here -- the name is what dedupes it.
// IF NOT EXISTS is belt and braces for a pre-existing db file.
Db.migrate("Create tasks table", `
  CREATE TABLE IF NOT EXISTS tasks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    done INTEGER NOT NULL DEFAULT 0,
    createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
  )
`)
