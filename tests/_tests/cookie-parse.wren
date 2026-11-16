// Cookie.parseHeader fills the cookie map from a request Cookie header.
Cookie.parseHeader("sid=abc; theme = dark ; junk")
Test.assert(Cookie.get("sid") == "abc", "parseHeader reads the first cookie")
Test.assert(Cookie.get("theme") == "dark", "parseHeader trims spaces around a value")
Test.assert(Cookie.get("junk") == null, "parseHeader skips a segment without '='")
Test.assert(Cookie.get("missing") == null, "parseHeader leaves unknown keys null")
Cookie.init
