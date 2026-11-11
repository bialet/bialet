Db.migrate("init", `
  CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    email TEXT UNIQUE NOT NULL,
    password TEXT NOT NULL
  );
  CREATE TABLE items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    sku TEXT,
    quantity INTEGER NOT NULL DEFAULT 0,
    notes TEXT,
    photo_id INTEGER,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
  );
`)

// Seed one account so the app is usable after `bialet` starts. Change the
// password before this leaves your machine.
var users = `SELECT COUNT(*) FROM users`.toNum
if (users == 0) {
  `users`.save({
    "email": "admin@example.com",
    "password": Util.hash("changeme")
  })
}
