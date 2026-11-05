// Reading an upload must not keep it. Request.file() used to call .save, which
// moved the row out of the temporary pool and out of the one-day purge, so a
// handler that read a file only to reject it grew the database forever.
var file = Request.file("form_file_name")
if (!file) return "none"

var isTempAfterRead = file.isTemp
var rowAfterRead = `SELECT isTemp FROM BIALET_FILES WHERE id = ?`.first([file.id])
file.save
var rowAfterSave = `SELECT isTemp FROM BIALET_FILES WHERE id = ?`.first([file.id])
file.destroy

return "isTemp:%(isTempAfterRead)|rowAfterRead:%(rowAfterRead["isTemp"])|rowAfterSave:%(rowAfterSave["isTemp"])"
