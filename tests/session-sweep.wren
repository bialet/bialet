// The session table is written by unauthenticated requests; its write path must
// bound the number of live session ids instead of letting every request add one.
Session.maxSessions = 2
var i = 0
while (i < 128) {
  Cookie.set(Session.name, "sweepid%(i % 3)")
  Session.new().set("k", "v")
  i = i + 1
}
var row = `SELECT COUNT(*) AS c FROM (SELECT id FROM BIALET_SESSION GROUP BY id)`.first()
var live = Num.fromString("%(row["c"])")
return "live:%(live <= 2 ? "bounded" : "unbounded")"
