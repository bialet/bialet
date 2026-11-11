// CSRF has class-level accessors and the Test client keeps a cookie jar, so a
// GET that renders a form and a POST that submits it share one session. Before
// the jar every Test call started a fresh session and csrfOk could never pass.

// Statics work without holding a Session instance.
Test.assert(Session.csrf.toString.contains("_bialet_csrf"), "Session.csrf renders a token")
Test.assert(Session.csrfOk == false, "Session.csrfOk is false without a posted token")

// The hidden token from the GET body plus the session cookie the jar stored
// must validate on the following POST.
var page = Test.get("/csrf").status(200)
var token = page.csrf()
Test.assert(token.count > 0, "GET renders a CSRF token")
Test.post("/csrf", {"_bialet_csrf": token}).status(200).equals("ok")

// A wrong token still fails, so the jar is not bypassing the check.
Test.post("/csrf", {"_bialet_csrf": "wrong"}).status(200).equals("fail")

// The jar can be inspected and cleared.
Test.assert(Test.cookie("BIALETSESSID") != null, "cookie jar stores the session id")
Test.clearCookies()
Test.assert(Test.cookie("BIALETSESSID") == null, "clearCookies empties the jar")
