`CREATE TABLE IF NOT EXISTS query_limit (id INTEGER PRIMARY KEY, label TEXT)`.query
`DELETE FROM query_limit`.query
`INSERT INTO query_limit (id, label) VALUES (1, 'first')`.query
`INSERT INTO query_limit (id, label) VALUES (2, 'second')`.query

// An explicit LIMIT is left untouched: .first() must not append a second one.
var existing = `SELECT id FROM query_limit ORDER BY id LIMIT 1`.first["id"]
// The word LIMIT in a string literal, a comment or a quoted identifier is not
// a LIMIT clause, so .first() still appends one.
var stringLiteral = Query.hasLimit_("SELECT 'LIMIT' AS word")
var lineComment = Query.hasLimit_("SELECT 1 -- LIMIT 1")
var blockComment = Query.hasLimit_("SELECT 1 /* LIMIT 1 */")
var quotedName = Query.hasLimit_("SELECT \"LIMIT\" FROM query_limit")
var subquery = Query.hasLimit_("SELECT * FROM (SELECT id FROM query_limit LIMIT 1)")
var realLimit = Query.hasLimit_("SELECT id FROM query_limit LIMIT 1")
// And the string-literal query itself still runs through .first().
var literalRow = `SELECT 'LIMIT' AS word, id FROM query_limit ORDER BY id`.first

return "%(existing)|%(stringLiteral.toString)|%(lineComment.toString)|%(blockComment.toString)|%(quotedName.toString)|%(subquery.toString)|%(realLimit.toString)|%(literalRow["word"])|%(literalRow["id"])"
