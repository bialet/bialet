`CREATE TABLE IF NOT EXISTS t_dbmore (id INTEGER PRIMARY KEY, name TEXT, value TEXT)`.query
`DELETE FROM t_dbmore`.query
`DELETE FROM BIALET_MIGRATIONS WHERE version = 't-dbmore-migration'`.query
`DELETE FROM BIALET_MIGRATIONS WHERE version = 't-dbmore-cb'`.query
`DELETE FROM BIALET_MIGRATIONS WHERE version = 't-dbmore-date'`.query

var inserted = Db.save("t_dbmore", {"name": "alpha", "value": "v1"})
Test.assert(inserted != null, "Db.save returns id")
Test.assert(`SELECT * FROM t_dbmore WHERE name = 'alpha'`.first["value"] == "v1", "Db.save inserted row")

Db.delete("t_dbmore", inserted)
Test.assert(`SELECT COUNT(*) FROM t_dbmore`.toNum == 0, "Db.delete removed row")

Db.migrate("t-dbmore-migration", `INSERT INTO t_dbmore (name, value) VALUES ('migrated', 'yes')`)
Test.assert(`SELECT COUNT(*) FROM t_dbmore WHERE name = 'migrated'`.toNum == 1, "Db.migrate ran")
Db.migrate("t-dbmore-migration", `INSERT INTO t_dbmore (name, value) VALUES ('again', 'no')`)
Test.assert(`SELECT COUNT(*) FROM t_dbmore WHERE name = 'migrated'`.toNum == 1, "Db.migrate runs once")

Db.migrate("t-dbmore-cb", Fn.new { `INSERT INTO t_dbmore (name, value) VALUES ('cb', 'ran')`.query })
Test.assert(`SELECT COUNT(*) FROM t_dbmore WHERE name = 'cb'`.toNum == 1, "Db.migrate callback")

// Db.migrate runs its callback after Db.init has initialized Date, so
// Date.new() and date arithmetic work there instead of crashing the process.
Db.migrate("t-dbmore-date", Fn.new {
  var today = Date.new()
  Test.assert(today is Date, "Date.new works in a migrate callback")
  Test.assert(today.year >= 2020, "migrate callback sees a real date")
  `INSERT INTO t_dbmore (name, value) VALUES ('dated', ?)`.query(today.toString)
})
Test.assert(`SELECT COUNT(*) FROM t_dbmore WHERE name = 'dated'`.toNum == 1, "migrate callback inserted a date")

// A statement that fails must surface as an error and must not record the
// version: Db.migrate uses that row to skip work, so a failed migration marked
// as applied could never be retried. This covers both the SQL-string form and
// a callback whose query fails.
`DROP TABLE IF EXISTS t_dbmore_fail`.query
var failingSql = Fiber.new {
  Db.migrate("t-dbmore-fail", "CREATE TABLE t_dbmore_fail (id INTEGER); CREATE TABLE broken (")
}
failingSql.try()
Test.assert(failingSql.error != null, "a failed migration raises an error")
Test.assert(`SELECT COUNT(*) FROM BIALET_MIGRATIONS WHERE version = 't-dbmore-fail'`.toNum == 0,
            "a failed migration is not recorded")

var failingCallback = Fiber.new {
  Db.migrate("t-dbmore-fail-cb", Fn.new { `INSERT INTO t_missing (x) VALUES (1)`.query })
}
failingCallback.try()
Test.assert(failingCallback.error != null, "a failed callback migration raises an error")
Test.assert(`SELECT COUNT(*) FROM BIALET_MIGRATIONS WHERE version = 't-dbmore-fail-cb'`.toNum == 0,
            "a failed callback migration is not recorded")

`INSERT INTO BIALET_SESSION (id, key, val, updatedAt) VALUES ('t-old', 'k', 'v', '2020-01-01')`.query
Db.clean
Test.assert(`SELECT COUNT(*) FROM BIALET_SESSION WHERE id = 't-old'`.toNum == 0, "Db.clean removed old session")
