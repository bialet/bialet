// A ';' inside a quoted default or a line comment is part of the statement,
// not a boundary. -t used to cut at that ';' and report the tail of the ALTER
// as a syntax error.
Db.migrate("alter with quoted semicolons", `
  ALTER TABLE not_created_yet ADD COLUMN note TEXT DEFAULT 'a;b';
  ALTER TABLE not_created_yet -- a ; comment
    ADD COLUMN other TEXT;
`)
