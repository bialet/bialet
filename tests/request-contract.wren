// Repeated form fields keep the last value; Request.body is present for
// urlencoded bodies but get() never reads it.
if (Request.isPost) {
  var last = Request.post("a")
  var bodyCount = Request.body.count
  var fromQuery = Request.get("a")
  return "%(last)|%(bodyCount)|%(fromQuery == null)"
}
return "get"
