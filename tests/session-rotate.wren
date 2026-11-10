// A Session instance must follow the live cookie after rotate(): the planted
// identifier's row is deleted, so the first post-login write has to land under
// the freshly minted identifier, not recreate the retired one.
var planted = "plantedtestid0001"
Cookie.set(Session.name, planted)
var skipped = Session.new()
Session.rotate()
skipped.set("user", "alice")
var plantedRows = `SELECT val FROM BIALET_SESSION WHERE id = ? AND key = ?`.fetch([planted, "user"])
var fresh = Cookie.get(Session.name)
var freshRows = `SELECT val FROM BIALET_SESSION WHERE id = ? AND key = ?`.fetch([fresh, "user"])
var plantedVal = (plantedRows && plantedRows.count > 0) ? plantedRows[0]["val"] : "none"
var freshVal = (freshRows && freshRows.count > 0) ? freshRows[0]["val"] : "none"
return "planted:%(plantedVal)|fresh:%(freshVal)"
