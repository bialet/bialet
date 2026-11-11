`CREATE TABLE IF NOT EXISTS nums_test (num INTEGER)`.query
`DELETE FROM nums_test`.query

// An aggregate over no rows is NULL in SQL. toNum coalesces it to 0, so it is
// safe to do arithmetic on the result.
var emptyMax = `SELECT MAX(num) FROM nums_test`.toNum
var emptySum = `SELECT SUM(num) FROM nums_test`.toNum

// A query that matches no row at all also has no first value.
var noRow = `SELECT num FROM nums_test WHERE 0`.toNum

`INSERT INTO nums_test (num) VALUES (42)`.query

var num = `SELECT num FROM nums_test`.toNum
return "empty:%(emptyMax)|%(emptySum)|missing:%(noRow)|%(num + 8)"
