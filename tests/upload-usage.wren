// Reports the aggregate temporary-upload state and the query plan the upload
// path uses for it. Used by the suite to check the retained set is bounded in
// rows as well as bytes and that the aggregate is served from the isTemp index
// instead of a full scan of BIALET_FILES.
var rows = `SELECT COUNT(*) FROM BIALET_FILES WHERE isTemp = 1`.toNum
var plan = ""
var steps = `EXPLAIN QUERY PLAN SELECT IFNULL(SUM(size), 0), COUNT(*) FROM BIALET_FILES WHERE isTemp = 1`.fetch
for (step in steps) plan = plan + " " + step.values.join(" ")
return "rows:%(rows)|index:%(plan.contains("BIALET_FILES_ISTEMP"))|scan:%(plan.contains("SCAN"))"
