// The real-world migration shape: a table that the validation database has
// not created yet, and no trailing ';' (so sqlite3_prepare_v2 stops at "ADD"
// and tail points inside the statement). Neither form may become a syntax
// error; the second migrate covers the multi-statement literal.
Db.migrate("alter a table the check has not created", `
  ALTER TABLE not_created_yet ADD COLUMN note TEXT
`)

Db.migrate("two alters in one literal", `
  ALTER TABLE not_created_yet ADD COLUMN other TEXT;
  ALTER TABLE not_created_yet ADD COLUMN more TEXT;
`)
