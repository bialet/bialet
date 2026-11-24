// The missing table is a schema error, but the ALTER itself is incomplete.
// Skipping to the ';' must not hide that syntax error.
Db.migrate("incomplete alter", `
  ALTER TABLE not_created_yet ADD COLUMN;
`)
