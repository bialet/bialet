var names = Markdown.list("md-dir").join(",")
var meta = Markdown.meta("md-dir/alpha.md")
var empty = Markdown.meta("md-dir/beta.markdown")
var missing = Markdown.meta("md-dir/missing.md")
var privateFile = Markdown.meta("md-dir/_private.md")
var traversal = Markdown.list("../").count
var privateDir = Markdown.list("_tests").count

var title = meta is Map ? meta["title"] : "error"
var category = meta is Map ? meta["category"] : "error"
var status = meta is Map ? meta["status"] : "error"
var metaCount = meta is Map ? meta.count : -1

return "%(names)|%(title)|%(category)|%(status)|%(metaCount)|%(empty.count)|%(missing.toString)|%(privateFile.toString)|%(traversal)|%(privateDir)"
