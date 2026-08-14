var sess = Session.new()
Test.assert(sess.id.count == 40, "Session id length")
Test.assert(Session.name == "BIALETSESSID", "Session default name")

Session.name = "TCOOKIE"
Test.assert(Session.name == "TCOOKIE", "Session rename")

sess.set("tk", "tv")
Test.assert(sess.get("tk") == "tv", "Session set/get")

// Writes must replace, not append: a key stays at exactly one row and
// get() keeps returning the latest value written.
sess.set("tk", "tv2")
sess.set("tk", "tv3")
Test.assert(`SELECT COUNT(*) FROM BIALET_SESSION WHERE id = ? AND key = 'tk'`.toNum([sess.id]) == 1,
            "Session set replaces row")
Test.assert(`SELECT val FROM BIALET_SESSION WHERE id = ? AND key = 'tk'`.val([sess.id]) == "tv3",
            "Session stores latest value")
Test.assert(sess.get("tk") == "tv3", "Session get returns latest")

// The session table must key on (id, key) so REPLACE never appends.
Test.assert(`SELECT COUNT(*) FROM pragma_table_info('BIALET_SESSION') WHERE pk > 0`.toNum >= 2,
            "Session table has a composite primary key")

Session.destroy()
Test.assert(Session.name == "TCOOKIE", "Session destroy keeps name")
