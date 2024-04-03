import "bialet" for Db

Db.migrate("Create Poll table", `
  CREATE TABLE poll (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    answer TEXT NOT NULL,
    comment TEXT,
    votes INTEGER NOT NULL DEFAULT 0
  )
`)

Db.migrate("Add initial data", `
  INSERT INTO poll (answer, comment) VALUES
    ("Yes", "We need to go back to the good old times"),
    ("No", "You are just old")
`)

Db.migrate("Add color column", `
  ALTER TABLE poll ADD COLUMN color TEXT
`)

Db.migrate("Add color data", `
  UPDATE poll SET color = "blue" WHERE answer = "Yes";
  UPDATE poll SET color = "red" WHERE answer = "No";
`)
