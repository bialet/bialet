// INSERT reports a missing column as "table t has no column named x"; after
// the dummy column is added, the syntax error in the VALUES list must still be
// reported.
Db.migrate("missing insert column then typo", `
  INSERT INTO not_created_yet (missing_column) VALUES (1,);
`)
