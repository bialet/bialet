// The maxSessions cap may only evict sessions that were never authenticated.
// Before rotate() marked the id it mints, the sweep deleted the ids with the
// oldest MAX(updatedAt), and those are the logged-in users' login-time rows, so
// an anonymous flood that adds one fresh id per request forced a global logout.
// Here the flood ids are newer than an aged login row, so the pre-fix eviction
// targets exactly the authenticated id.
Session.maxSessions = 8
// Drop the flood ids an earlier run of this test left behind, so the counts
// below only describe this request.
`DELETE FROM BIALET_SESSION WHERE id LIKE 'evictflood%'`.query()
var authed = Session.rotate()
// Age the login row like a real session left alone since login: login-time age
// was the only signal the cap had.
`UPDATE BIALET_SESSION SET updatedAt = datetime('now', '-1 hour') WHERE id = ?`.query([authed])

var i = 0
while (i < 40) {
  Cookie.set(Session.name, "evictflood%(i)")
  Session.new().set("k", "v")
  i = i + 1
}
// The sweep trigger is probabilistic, so run it once directly: the assertion is
// about the eviction rule, not about how often the trigger fires.
Session.sweep_()

var authedRow = `SELECT COUNT(*) AS c FROM BIALET_SESSION WHERE id = ?`.first([authed])
var authedRows = Num.fromString("%(authedRow["c"])")
var floodRow = `SELECT COUNT(*) AS c FROM (SELECT id FROM BIALET_SESSION WHERE id LIKE 'evictflood%' GROUP BY id)`.first()
var floodIds = Num.fromString("%(floodRow["c"])")
return "authed:%(authedRows > 0 ? "yes" : "no")|evicted:%(floodIds < 40 ? "yes" : "no")"
