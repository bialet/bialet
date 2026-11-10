var target = Request.get("target")
var h = Http.new()
var ok = h.call(target + "/echo?mode=cookie", {"headers": {"Cookie": "a=1"}})
return "ok:%(ok)|err:%(h.error)|msg:%(h.errorMessage)|body:%(h.body)"
