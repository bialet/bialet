var t1 = "  hello  ".trim()
var t2 = "\t\ntrim\r\n".trim()
var t3 = "no-whitespace".trim()
var t4 = "   ".trim()
var t5 = "abc123".trim("abc")
var t6 = "  trimleft".trimStart()
var t7 = "trimright  ".trimEnd()
var t8 = "xxxhelloxxx".trim("x")

return t1 + "," + t2 + "," + t3 + "," + t4 + "," + t5 + "," + t6 + "," + t7 + "," + t8
