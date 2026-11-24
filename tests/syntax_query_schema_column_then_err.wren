// A missing column is repaired against a dummy table; the syntax error after
// the column must still be reported.
Db.migrate("missing column then typo", `
  UPDATE not_created_yet SET missing_column = 1 WHERE;
`)
