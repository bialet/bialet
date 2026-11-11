// The session table is written by unauthenticated requests, and the sweep that
// bounds it runs on the write path. Its trigger must survive the fresh VM built
// for every request, and the draws are probabilistic (about one write in 62).
// So: create many ids, backdate them, then hold one id for enough writes that a
// sweep is a practical certainty. Writes after a sweep reuse the live id, so the
// count can only fall; with a dead trigger the ids would stay and the aged rows
// would survive.
Session.maxSessions = 2

var i = 0
while (i < 50) {
  Cookie.set(Session.name, "sweepid%(i)")
  Session.new().set("k", "v")
  i = i + 1
}
`UPDATE BIALET_SESSION SET updatedAt = datetime('now', '-60 days')`.query()

Cookie.set(Session.name, "stableseed")
Session.new().set("k", "v")
i = 0
while (i < 1000) {
  Session.new().set("k", "v")
  i = i + 1
}

var liveRow = `SELECT COUNT(*) AS c FROM (SELECT id FROM BIALET_SESSION GROUP BY id)`.first()
var live = Num.fromString("%(liveRow["c"])")
var agedRow = `SELECT COUNT(*) AS c FROM BIALET_SESSION WHERE updatedAt < datetime('now', '-30 days')`.first()
var aged = Num.fromString("%(agedRow["c"])")
return "live:%(live <= 2 ? "bounded" : "unbounded")|old:%(aged)"
