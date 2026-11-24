// A valid trigger body that references the table -t has not created. The
// schema error before the body must not turn into a syntax error, and the body
// itself must still be parsed.
Db.migrate("trigger on a missing table", `
  CREATE TRIGGER tr AFTER INSERT ON not_created_yet BEGIN
    UPDATE not_created_yet SET note = NEW.note;
  END;
`)
