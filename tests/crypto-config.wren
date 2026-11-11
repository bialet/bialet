// Config.encrypt / Config.decrypt under the deployment key loaded with
// --secret-key-file. The key itself never reaches Wren, so the only thing an
// application can observe about it is Secret.available.
var checks = []
checks.add(Secret.available)

Config.encrypt("crypto_secret", "sk_live_1234567890")
var stored = Config.get("crypto_secret")
checks.add(stored != null)
checks.add(stored[0...6] == "bgcm1$")
checks.add(Config.decrypt("crypto_secret") == "sk_live_1234567890")

// The config key name is authenticated with the value: the same ciphertext
// copied under another key must not decrypt. That turns "move the admin key
// into a field rendered on a public page" into a visible failure.
Config.set("crypto_copy", stored)
checks.add(Config.decrypt("crypto_copy") == null)

// A row that was never encrypted, and a missing row, are null rather than a 500.
Config.set("crypto_plain", "hello")
checks.add(Config.decrypt("crypto_plain") == null)
checks.add(Config.decrypt("crypto_missing") == null)

// The explicit-secret form is bound to the config key in the same way.
Config.encrypt("crypto_explicit", "value", "passphrase")
checks.add(Config.decrypt("crypto_explicit", "passphrase") == "value")
checks.add(Config.decrypt("crypto_explicit", "other") == null)

var failed = checks.where {|ok| !ok }.count
return failed == 0 ? "true" : "%(failed) of %(checks.count) checks failed"
