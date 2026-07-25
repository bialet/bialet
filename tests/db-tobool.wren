`CREATE TABLE IF NOT EXISTS tobool_test (id INTEGER PRIMARY KEY, flag INTEGER)`.query
`DELETE FROM tobool_test`.query
`INSERT INTO tobool_test (id, flag) VALUES (1, 1)`.query
`INSERT INTO tobool_test (id, flag) VALUES (2, 0)`.query
`INSERT INTO tobool_test (id, flag) VALUES (3, -1)`.query
`INSERT INTO tobool_test (id, flag) VALUES (4, 5)`.query

var r1 = `SELECT flag FROM tobool_test WHERE id = 1`.toBool.toString
var r2 = `SELECT flag FROM tobool_test WHERE id = 2`.toBool.toString
var r3 = `SELECT flag FROM tobool_test WHERE id = 3`.toBool.toString
var r4 = `SELECT flag FROM tobool_test WHERE id = 4`.toBool.toString
var r5 = `SELECT flag FROM tobool_test WHERE id = ?`.toBool(1).toString

return r1 + "," + r2 + "," + r3 + "," + r4 + "," + r5
