// Request.body is the framed body byte for byte: a LF must survive.
var b = Request.body
return "len:%(b.byteCount_)|lf:%(b.contains("\n"))|cr:%(b.contains("\r"))"
