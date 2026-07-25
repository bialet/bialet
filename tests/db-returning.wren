`CREATE TABLE IF NOT EXISTS returning_test (id INTEGER PRIMARY KEY, counter INTEGER)`.query
`DELETE FROM returning_test`.query
`INSERT INTO returning_test (id, counter) VALUES (1, 0)`.query

var updated = `UPDATE returning_test SET counter = counter + 5 WHERE id = 1 RETURNING counter`.val
var current = `SELECT counter FROM returning_test WHERE id = 1`.val
var sum = `SELECT SUM(counter) FROM returning_test`.toNum

return updated + "," + current + "," + sum.toString
