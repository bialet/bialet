var sess = Session.new()
var sid = sess.id
var defaultName = Session.name
Session.name = "MYTESTCOOKIE"
var newName = Session.name
sess.set("meta_key", "meta_value")
var got = sess.get("meta_key")
Session.destroy()
return "sidLen:" + sid.count.toString + "|default:" + defaultName + "|renamed:" + newName + "|got:" + got
