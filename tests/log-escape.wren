// App-supplied log text is escaped at the framework's log sink: a request value
// logged with System.log must not be able to open a second record or emit a
// terminal escape, whether it arrives in the query string, a form field or an
// error message.
var value = Request.post("v")
if (value) System.log("probe %(value)")
return "logged"
