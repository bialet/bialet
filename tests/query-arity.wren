`CREATE TABLE IF NOT EXISTS query_arity (a TEXT, b TEXT, c TEXT, d TEXT, e TEXT, f TEXT, g TEXT)`.query
`DELETE FROM query_arity`.query

`INSERT INTO query_arity (a, b, c, d, e, f, g) VALUES (?, ?, ?, ?, ?, ?, ?)`.query("1", "2", "3", "4", "5", "6", "7")
`UPDATE query_arity SET a = ?, b = ?, c = ?, d = ?, e = ?, f = ?, g = ?`.query("1", "2", "3", "4", "5", "6", "7")

var row = `SELECT a, b, c, d, e, f, g FROM query_arity WHERE a = ? AND b = ? AND c = ? AND d = ? AND e = ? AND f = ? AND g = ?`.first("1", "2", "3", "4", "5", "6", "7")
var joined = row["a"] + row["b"] + row["c"] + row["d"] + row["e"] + row["f"] + row["g"]
var fetched = `SELECT g FROM query_arity WHERE a = ? AND b = ? AND c = ? AND d = ? AND e = ? AND f = ? AND g = ?`.fetch("1", "2", "3", "4", "5", "6", "7").count
var value = `SELECT a FROM query_arity WHERE a = ? AND b = ? AND c = ? AND d = ? AND e = ? AND f = ? AND g = ?`.val("1", "2", "3", "4", "5", "6", "7")
var number = `SELECT g FROM query_arity WHERE a = ? AND b = ? AND c = ? AND d = ? AND e = ? AND f = ? AND g = ?`.toNum("1", "2", "3", "4", "5", "6", "7")
var flag = `SELECT 1 FROM query_arity WHERE a = ? AND b = ? AND c = ? AND d = ? AND e = ? AND f = ? AND g = ?`.toBool("1", "2", "3", "4", "5", "6", "7")
var empty = `SELECT a FROM query_arity WHERE a = ? AND b = ?`.val("missing", "missing")

return "%(joined)|%(fetched)|%(value)|%(number)|%(flag)|%(empty == null)"
