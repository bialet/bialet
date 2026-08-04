var target = Request.get("target")
var post = Http.post(target + "/echo", {"k": "v"})
var put = Http.put(target + "/echo", {"k": "v"})
var del = Http.delete(target + "/echo")
var get = Http.get(target + "/echo", {})
var req = Http.request(target + "/echo", "GET", null, {})

return post + "|" + put + "|" + del + "|" + get + "|" + req
