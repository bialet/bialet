var hex = Util.hexToDec("FF")
var hexLower = Util.hexToDec("1a")
var toHex = Util.toHex(255)
var lpad = Util.lpad("7", 3, "0")
var rev = Util.reverse("abc")
var esc = Util.htmlEscape("<b>&\"'</b>")
var enc = Util.urlEncode("a b&c=d?e")
var dec = Util.urlDecode("a+b\%26c\%3Dd")
var eqOk = Util.secureEquals("abc", "abc")
var eqNo = Util.secureEquals("abc", "abd")
var eqNull = Util.secureEquals(null, "abc")
var pos = Util.getPositionForIndex("line1\nline2", 7)
var toNum = Util.toNum("42")
var toNumBad = Util.toNum("nope")
var toNumNum = Util.toNum(7)

return "hex:" + hex.toString +
  "|hexLower:" + hexLower.toString +
  "|toHex:" + toHex +
  "|lpad:" + lpad +
  "|rev:" + rev +
  "|esc:" + esc +
  "|enc:" + enc +
  "|dec:" + dec +
  "|eq:" + eqOk.toString + "," + eqNo.toString + "," + eqNull.toString +
  "|pos:" + pos["line"].toString + "," + pos["column"].toString +
  "|num:" + toNum.toString + "," + toNumBad.toString + "," + toNumNum.toString
