// A session expires when the whole id has been idle for 30 days, not when any
// one of its rows is old. rotate() writes the _bialet_authenticated marker once
// and nothing refreshes it, so an "any row is old" expiry turned the idle
// timeout into a hard 30-day lifetime: a session used every day was deleted on
// day 31 by its own marker row.
//
// Here the marker row is 40 days old and the app row was written just now, which
// is exactly the shape a long-lived, actively used login produces. The id must
// survive the sweep; the sibling case below (every row old) must still expire.
Session.maxSessions = 1000000
`DELETE FROM BIALET_SESSION WHERE id LIKE 'expiry%'`.query()

var active = Session.rotate()
`UPDATE BIALET_SESSION SET updatedAt = datetime('now', '-40 days') WHERE id = ? AND key = '_bialet_authenticated'`.query([active])
Session.new().set("k", "v")
Session.sweep_()
var kept = `SELECT COUNT(*) AS c FROM BIALET_SESSION WHERE id = ?`.first([active])
var keptRows = Num.fromString("%(kept["c"])")

// Control: an id whose *every* row is older than 30 days must be gone.
var idle = "expirytestidle"
`REPLACE INTO BIALET_SESSION (id, key, val, updatedAt) VALUES (?, '_bialet_authenticated', '1', datetime('now', '-40 days'))`.query([idle])
`REPLACE INTO BIALET_SESSION (id, key, val, updatedAt) VALUES (?, 'k', 'v', datetime('now', '-31 days'))`.query([idle])
Session.sweep_()
var gone = `SELECT COUNT(*) AS c FROM BIALET_SESSION WHERE id = ?`.first([idle])
var goneRows = Num.fromString("%(gone["c"])")

return "active:%(keptRows > 0 ? "yes" : "no")|idle:%(goneRows == 0 ? "yes" : "no")"
