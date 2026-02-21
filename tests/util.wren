var hash = Util.hash("password123")
var verified = Util.verify("password123", hash)
var random = Util.randomString(10)
var encoded = "hello+world"
var decoded = Util.urlDecode(encoded)
var num = Util.toNum("42")

Response.out(verified.toString + "," + (random.count == 10).toString + "," + decoded + "," + num.toString)
