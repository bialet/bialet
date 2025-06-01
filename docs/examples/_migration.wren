import "bialet" for Db

Db.migrate("Users Table", `CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)`)
Db.migrate("Users Data", `REPLACE INTO users (id, name) VALUES (1, "Alice"), (2, "Bob"), (3, "Charlie")`)
