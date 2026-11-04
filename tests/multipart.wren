if (Request.isPost) {
  if (Request.get("unnamed")) return Request.file("") ? "unnamed" : "named"
  var out = []
  for (k in ["a", "b", "c", "note", "title", "excel", "up", "empty"]) {
    var v = Request.post(k)
    out.add("%(k)=%(v == null ? "null" : v)")
  }
  var f = Request.file("excel")
  out.add("file=%(f ? "%(f.name):%(f.size)" : "none")")
  return out.join("|")
}
return "get"
