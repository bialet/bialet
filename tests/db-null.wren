`CREATE TABLE IF NOT EXISTS dbnull_test (id INTEGER PRIMARY KEY, val TEXT)`.query
`DELETE FROM dbnull_test`.query
`INSERT INTO dbnull_test (val) VALUES (NULL)`.query
`INSERT INTO dbnull_test (val) VALUES (?)`.query(null)

var literalNull = `SELECT val FROM dbnull_test WHERE id = 1`.first
var boundNull = `SELECT val FROM dbnull_test WHERE id = 2`.first

return "literal:" + (literalNull["val"] == null).toString +
  "|bound:" + (boundNull["val"] == null).toString
