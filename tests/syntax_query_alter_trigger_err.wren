// The trigger is on a table -t has not created, so prepare reports the schema
// error first. The broken body after it is still a syntax error and must be
// reported instead of being skipped up to the first ';'.
Db.migrate("trigger body typo", `
  CREATE TRIGGER tr AFTER INSERT ON not_created_yet BEGIN
    SELEC * FROM t;
  END;
`)
