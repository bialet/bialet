`CREATE TABLE IF NOT EXISTS values_test (val TEXT)`.query
`DELETE FROM values_test`.query
`INSERT INTO values_test (val) VALUES ('testvalue')`.query

var value = `SELECT val FROM values_test`.val
Response.out(value)
