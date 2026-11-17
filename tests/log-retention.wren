// BIALET_LOGS had no retention and no record cap: every log line stayed in the
// app database forever, at up to four times its escaped size.
`INSERT INTO BIALET_LOGS (message, createdAt) VALUES ('logretention-old', datetime('now', '-40 days'))`.query()
Db.clean
var old = `SELECT COUNT(*) AS c FROM BIALET_LOGS WHERE message = 'logretention-old'`.first()
var big = ""
for (i in 0...5000) big = big + "x"
System.log("logretention-big" + big)
var row = `SELECT LENGTH(message) AS n FROM BIALET_LOGS WHERE message LIKE 'logretention-big%' ORDER BY rowid DESC`.first()
var n = Num.fromString("%(row["n"])")
return "old:%(old["c"])|capped:%(n <= 4099)"
