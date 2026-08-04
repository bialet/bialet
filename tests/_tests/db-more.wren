`CREATE TABLE IF NOT EXISTS t_dbmore (id INTEGER PRIMARY KEY, name TEXT, value TEXT)`.query
`DELETE FROM t_dbmore`.query
`DELETE FROM BIALET_MIGRATIONS WHERE version = 't-dbmore-migration'`.query
`DELETE FROM BIALET_MIGRATIONS WHERE version = 't-dbmore-cb'`.query

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

`INSERT INTO BIALET_SESSION (id, key, val, updatedAt) VALUES ('t-old', 'k', 'v', '2020-01-01')`.query
Db.clean
Test.assert(`SELECT COUNT(*) FROM BIALET_SESSION WHERE id = 't-old'`.toNum == 0, "Db.clean removed old session")
