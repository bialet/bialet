
Db.migrate("Counter Table", `CREATE TABLE IF NOT EXISTS counter (name TEXT PRIMARY KEY, value INTEGER)`)
Db.migrate("Items Table", `CREATE TABLE IF NOT EXISTS items (phrase TEXT)`)
Db.migrate("Users Table", `CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)`)
Db.migrate("Users Data", `REPLACE INTO users (id, name) VALUES (1, "Alice"), (2, "Bob"), (3, "Charlie")`)
