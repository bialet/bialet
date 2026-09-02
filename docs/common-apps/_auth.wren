// Login helpers. The single user and the password hash live in the
// application config store (BIALET_CONFIG) under `login_user` and
// `login_password`. Sessions are cookie-based (the built-in Session class).
class Auth {
  // Returns null when the visitor is signed in, otherwise a redirect. Call it
  // first on every protected page and return its result when it is not null.
  static require {
    var sess = Session.new()
    if (sess.get("loggedIn") == "1") return null
    return Response.redirect("/login")
  }

  static user {
    var sess = Session.new()
    var name = sess.get("loginUser")
    return name != null ? name : null
  }

  // Verifies a submitted user/password pair against the config values.
  // The stored password is a salted SHA-256 hash produced by Util.hash().
  static attempt(user, pass) {
    var expectedUser = Config.get("login_user")
    var expectedHash = Config.get("login_password")
    if (expectedUser == null || expectedHash == null) return false
    if (!Util.secureEquals(user, expectedUser)) return false
    return Util.verify(pass, expectedHash)
  }
}
