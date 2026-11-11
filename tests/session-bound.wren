// One session write per request: the shape Session.csrf produces on any form
// page. The sweep trigger must survive the fresh VM each request builds, or the
// table grows one row per request and neither the 30-day expiry nor
// maxSessions ever runs.
if (Request.get("report")) {
  var row = `SELECT COUNT(*) AS c FROM (SELECT id FROM BIALET_SESSION GROUP BY id)`.first()
  return "%(row["c"])"
}

Session.maxSessions = 1
Session.new().set("k", "v")
return "wrote"
