// Util.encrypt / Util.decrypt: AES-256-GCM with an explicit secret. No
// deployment key is involved, so this also runs against an already-running
// server.
var checks = []

var sealed = Util.encrypt("honey", "password")
var sealedAgain = Util.encrypt("honey", "password")

checks.add(Util.decrypt(sealed, "password") == "honey")
// Passphrase envelopes say which KDF and iteration count produced them.
checks.add(sealed.contains("$600000$"))
// Fresh salt and nonce every time: identical plaintext is not identical output.
checks.add(sealed != sealedAgain)
checks.add(Util.decrypt(sealedAgain, "password") == "honey")

// Wrong secret, edited ciphertext, truncated value, plain text: all null, and
// all indistinguishable from each other.
checks.add(Util.decrypt(sealed, "password2") == null)
var lastChar = sealed[sealed.count - 1]
var tampered = sealed[0...sealed.count - 1] + (lastChar == "0" ? "1" : "0")
checks.add(Util.decrypt(tampered, "password") == null)
checks.add(Util.decrypt(sealed[0...40], "password") == null)
checks.add(Util.decrypt("not encrypted", "password") == null)
checks.add(Util.decrypt("", "password") == null)
checks.add(Util.decrypt(null, "password") == null)
checks.add(Util.decrypt(sealed, null) == null)

// A 64-hex-character secret is a raw AES-256 key and skips the KDF.
var rawKey = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
var rawSealed = Util.encrypt("honey", rawKey)
checks.add(rawSealed.contains("$0$-"))
checks.add(Util.decrypt(rawSealed, rawKey) == "honey")
checks.add(Util.decrypt(rawSealed, "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1e") == null)

// Payloads are length-delimited, not C strings: NUL bytes and empty values
// survive, and so does non-ASCII.
var binary = ""
for (i in 0...32) binary = binary + String.fromByte(i)
checks.add(Util.decrypt(Util.encrypt(binary, "password"), "password") == binary)
checks.add(Util.decrypt(Util.encrypt("", "password"), "password") == "")
checks.add(Util.decrypt(Util.encrypt("contraseña 🚲", "password"), "password") == "contraseña 🚲")

var failed = checks.where {|ok| !ok }.count
return failed == 0 ? "true" : "%(failed) of %(checks.count) checks failed"
