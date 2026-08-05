// Migration. Bialet runs this at startup. No migrations folder, no
// version files — just a named script. Feels like a step back from
// knex/Prisma, but it's honest about being simple.
Db.migrate("tasks", `
  CREATE TABLE IF NOT EXISTS tasks (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    text       TEXT    NOT NULL,
    done       INTEGER NOT NULL DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
  )`)
