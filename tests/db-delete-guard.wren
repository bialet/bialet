// Db.delete builds its statement by interpolating the table name, so it must
// validate that name and refuse the framework's own tables, exactly like
// Db.save/Query.save do. Without the guard a request-derived record type could
// close the identifier and delete arbitrary rows, BIALET_* rows included.
`CREATE TABLE IF NOT EXISTS dbdel_victim (id INTEGER PRIMARY KEY, name TEXT)`.query
`CREATE TABLE IF NOT EXISTS dbdel_other (id INTEGER PRIMARY KEY, name TEXT)`.query
`DELETE FROM dbdel_victim`.query
`DELETE FROM dbdel_other`.query
`DELETE FROM BIALET_SESSION WHERE id = 'dbdel-test'`.query
`INSERT INTO BIALET_SESSION (id, key, val, updatedAt) VALUES ('dbdel-test', 'k', 'v', datetime('now'))`.query

for (i in 1..2) {
  Db.save("dbdel_victim", {"name": "v%(i)"})
  Db.save("dbdel_other", {"name": "o%(i)"})
}

var results = []

// A literal table name still deletes exactly one row.
var id = `SELECT id FROM dbdel_victim ORDER BY id`.val
Db.delete("dbdel_victim", id)
results.add("literal:" + `SELECT COUNT(*) FROM dbdel_victim`.toNum.toString)

// A payload that closes the identifier aborts instead of deleting every row.
var fiber = Fiber.new {
  Db.delete("dbdel_victim` WHERE 1=1; --", 999)
}
fiber.try()
results.add("aborted:" + (fiber.error != null).toString)
results.add("intact:" + `SELECT COUNT(*) FROM dbdel_victim`.toNum.toString)
results.add("other:" + `SELECT COUNT(*) FROM dbdel_other`.toNum.toString)

// A framework table is refused even as a plain identifier.
results.add("refused:" + Db.delete("BIALET_SESSION", 1).toString)
results.add("session:" + `SELECT COUNT(*) FROM BIALET_SESSION WHERE id = 'dbdel-test'`.toNum.toString)

`DELETE FROM dbdel_victim`.query
`DELETE FROM dbdel_other`.query
`DELETE FROM BIALET_SESSION WHERE id = 'dbdel-test'`.query

return results.join("|")
