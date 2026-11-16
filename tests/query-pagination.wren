`DROP TABLE IF EXISTS t_page`.query
`CREATE TABLE t_page (n INTEGER, m INTEGER)`.query
for (i in 1...6) {
  `INSERT INTO t_page (n, m) VALUES (?, ?)`.query(i, i)
}
// Four bound values: the positional overloads stop at three, so this must go
// through the list form. LIMIT/OFFSET placeholders bind the same way.
var four = `SELECT COUNT(*) FROM t_page WHERE n >= ? AND n <= ? AND m >= ? AND m <= ?`.fetch([1, 5, 1, 5])
var page = `SELECT n FROM t_page ORDER BY n LIMIT ? OFFSET ?`.fetch([2, 2])
return "four:%(four.count == 1 ? four[0]["COUNT(*)"] : -1)|page:%(page.count)|first:%(page.count > 0 ? page[0]["n"] : -1)"
