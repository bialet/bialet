// After a schema error inside a statement, the rest of that statement is
// skipped up to its ';' — but a real syntax error in the *next* statement of
// the same literal still has to be reported.
Db.migrate("schema error then a typo", `
  ALTER TABLE not_created_yet ADD COLUMN note TEXT;
  SELEC * FROM t
`)
