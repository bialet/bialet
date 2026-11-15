// A header value is framed by the request length, not by a NUL. The server used
// to hand the whole non-multipart message to Wren with wrenSetSlotString(),
// which truncates at the first embedded NUL, so the app saw a prefix while the
// C-side header scanner saw the rest of the line.
var value = Request.header("x-bin") || ""
var out = []
out.add("len:" + value.count.toString)
out.add("hasNul:" + (value.indexOf(String.fromByte(0)) >= 0).toString)
out.add("tail:" + (value.endsWith("z") ? "z" : "none"))
return out.join("|")
