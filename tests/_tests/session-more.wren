var sess = Session.new()
Test.assert(sess.id.count == 40, "Session id length")
Test.assert(Session.name == "BIALETSESSID", "Session default name")

Session.name = "TCOOKIE"
Test.assert(Session.name == "TCOOKIE", "Session rename")

sess.set("tk", "tv")
Test.assert(sess.get("tk") == "tv", "Session set/get")

Session.destroy()
Test.assert(Session.name == "TCOOKIE", "Session destroy keeps name")
