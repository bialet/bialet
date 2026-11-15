// Reports the aggregate temporary-upload state and the query plans the upload
// path uses for it. Used by the suite to check the retained set is bounded in
// rows as well as bytes, that the aggregate is served from the composite
// (isTemp, createdAt, id) index instead of a full scan of BIALET_FILES, and
// that the eviction reads rows in index order instead of sorting the pool.
var rows = `SELECT COUNT(*) FROM BIALET_FILES WHERE isTemp = 1`.toNum
var plan = ""
var steps = `EXPLAIN QUERY PLAN SELECT IFNULL(SUM(size), 0), COUNT(*) FROM BIALET_FILES WHERE isTemp = 1`.fetch
for (step in steps) plan = plan + " " + step.values.join(" ")
var evictPlan = ""
var evictSteps = `EXPLAIN QUERY PLAN SELECT id, IFNULL(size, 0) FROM BIALET_FILES WHERE isTemp = 1 AND id < ? ORDER BY createdAt ASC, id ASC LIMIT 1`.fetch([0])
for (step in evictSteps) evictPlan = evictPlan + " " + step.values.join(" ")
var evictIndexed = evictPlan.contains("BIALET_FILES_TEMP_AGE") && !evictPlan.contains("TEMP B-TREE")
return "rows:%(rows)|index:%(plan.contains("BIALET_FILES_TEMP_AGE"))|scan:%(plan.contains("SCAN"))|evict:%(evictIndexed)"
