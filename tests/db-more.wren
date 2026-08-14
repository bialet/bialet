`CREATE TABLE IF NOT EXISTS dbmore_test (id INTEGER PRIMARY KEY, name TEXT, value TEXT)`.query
`DELETE FROM dbmore_test`.query
`DELETE FROM BIALET_MIGRATIONS WHERE version = 'dbmore-migration'`.query
`DELETE FROM BIALET_MIGRATIONS WHERE version = 'dbmore-cb'`.query

var inserted = Db.save("dbmore_test", {"name": "alpha", "value": "v1"})
var row1 = `SELECT * FROM dbmore_test WHERE name = 'alpha'`.first
var count1 = `SELECT COUNT(*) FROM dbmore_test`.toNum

Db.delete("dbmore_test", inserted)
var count2 = `SELECT COUNT(*) FROM dbmore_test`.toNum

Db.migrate("dbmore-migration", `INSERT INTO dbmore_test (name, value) VALUES ('migrated', 'yes')`)
var migrated = `SELECT * FROM dbmore_test WHERE name = 'migrated'`.first
Db.migrate("dbmore-migration", `INSERT INTO dbmore_test (name, value) VALUES ('migrated-again', 'no')`)
var once = `SELECT COUNT(*) FROM dbmore_test WHERE name LIKE 'migrated%'`.toNum

Db.migrate("dbmore-cb", Fn.new { `INSERT INTO dbmore_test (name, value) VALUES ('cb', 'ran')`.query })
var cb = `SELECT * FROM dbmore_test WHERE name = 'cb'`.first

`INSERT INTO BIALET_SESSION (id, key, val, updatedAt) VALUES ('dbmore-old', 'k', 'v', '2020-01-01')`.query
Db.clean
var oldGone = `SELECT COUNT(*) FROM BIALET_SESSION WHERE id = 'dbmore-old'`.toNum

return "inserted:" + inserted +
  "|row1:" + row1["value"] +
  "|count1:" + count1.toString +
  "|count2:" + count2.toString +
  "|migrated:" + migrated["value"] +
  "|once:" + once.toString +
  "|cb:" + cb["value"] +
  "|oldGone:" + oldGone.toString
