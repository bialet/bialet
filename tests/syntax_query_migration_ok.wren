Db.migrate("query syntax check", `
  CREATE TABLE query_syntax_check (id INTEGER);
  INSERT INTO query_syntax_check VALUES (1);
`)
