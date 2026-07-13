`CREATE TABLE IF NOT EXISTS nums_test (num INTEGER)`.query
`DELETE FROM nums_test`.query
`INSERT INTO nums_test (num) VALUES (42)`.query

var num = `SELECT num FROM nums_test`.toNum
return (num + 8).toString
