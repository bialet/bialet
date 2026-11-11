// A logout deletes the caller's own id and nothing else. destroy() used to
// carry "OR updatedAt < date('now','-1 month')", so any principal's logout
// expired every session whose login-time rows were older than a month: it
// removed other users' user_id row and their _bialet_authenticated marker, and
// the marker is what stops an anonymous flood from evicting the id.
//
// The victim below is shaped like the documented login (rotate() writes the
// marker once, then the app writes user_id once) and is aged past a month. An
// unrelated id logs out; every victim row must still be there.
`DELETE FROM BIALET_SESSION WHERE id LIKE 'destroytest%'`.query()
var victim = "destroytestvictim000000000000000001"
`REPLACE INTO BIALET_SESSION (id, key, val, updatedAt) VALUES (?, '_bialet_authenticated', '1', datetime('now', '-40 days'))`.query([victim])
`REPLACE INTO BIALET_SESSION (id, key, val, updatedAt) VALUES (?, 'user_id', 'alice', datetime('now', '-40 days'))`.query([victim])

Cookie.set(Session.name, "destroytestcaller000000000000000002")
Session.new().set("k", "v")
Session.destroy()

var row = `SELECT COUNT(*) AS c FROM BIALET_SESSION WHERE id = ?`.first([victim])
var rows = Num.fromString("%(row["c"])")
var marker = `SELECT COUNT(*) AS c FROM BIALET_SESSION WHERE id = ? AND key = '_bialet_authenticated'`.first([victim])
var markers = Num.fromString("%(marker["c"])")
var mine = `SELECT COUNT(*) AS c FROM BIALET_SESSION WHERE id = 'destroytestcaller000000000000000002'`.first()
var myRows = Num.fromString("%(mine["c"])")

return "victim:%(rows == 2 ? "kept" : "lost")|marker:%(markers == 1 ? "kept" : "lost")|self:%(myRows == 0 ? "gone" : "kept")"
