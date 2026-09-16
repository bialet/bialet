// SHA-256, FIPS 180-4 / NIST vectors.
Test.assert(
  Util.sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
  "sha256 empty")
Test.assert(
  Util.sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
  "sha256 abc")
Test.assert(
  Util.sha256("The quick brown fox jumps over the lazy dog") ==
    "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592",
  "sha256 quick brown fox")

// Multi-block input (> 64 bytes) exercises the padding/length path.
var eighty = List.filled(80, "a").join("")
Test.assert(
  Util.sha256(eighty) == "0f45e858fbc4176cdf4e411f88281edefc390ae5afe7df0f44cd9297f0a64580",
  "sha256 multi-block")

// UTF-8 bytes, not code points.
Test.assert(
  Util.sha256("áéí") == "05dfd01f4186e528bbbe52f0ad16c72d521d6f366f60ad17b15c13945fa3d988",
  "sha256 utf8")

// The stored byte length is hashed, so an embedded NUL is not a terminator.
var withNul = "a" + String.fromByte(0) + "b"
Test.assert(
  Util.sha256(withNul) == "59b271ae1bbcb1d31d41929817f4b16fb439eb4f31520b5ad1d5ce98920a7138",
  "sha256 embedded NUL")

// Non-string arguments are coerced like the other Util helpers.
Test.assert(Util.sha256(123) == Util.sha256("123"), "sha256 coercion")

// PKCE S256 (RFC 7636 Appendix B).
Test.assert(
  Util.sha256Base64Url("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk") ==
    "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM",
  "PKCE code_challenge S256")

// encodeBase64 regression: it used to abort on String.byteAt.
Test.assert(Util.encodeBase64("abc") == "YWJj", "encodeBase64 abc")
Test.assert(Util.encodeBase64("a") == "YQ==", "encodeBase64 one byte")
Test.assert(Util.encodeBase64("ab") == "YWI=", "encodeBase64 two bytes")
Test.assert(Util.encodeBase64("") == "", "encodeBase64 empty")
Test.assert(Util.decodeBase64("YWJj") == "abc", "decodeBase64 abc")

// Round-trip, including multi-byte UTF-8.
for(sample in ["", "a", "ab", "abc", "héllo wörld ✓"]) {
  Test.assert(Util.decodeBase64(Util.encodeBase64(sample)) == sample,
              "base64 round-trip '%(sample)'")
}

// base64url: no padding, URL-safe alphabet.
Test.assert(Util.base64UrlEncode("a") == "YQ", "base64UrlEncode no padding")
Test.assert(Util.base64UrlEncode("abc") == "YWJj", "base64UrlEncode abc")
var urlSafe =
  Util.base64UrlEncode(String.fromByte(0xFB) + String.fromByte(0xEF) + String.fromByte(0xFF))
Test.assert(!urlSafe.contains("+") && !urlSafe.contains("/") && !urlSafe.contains("="),
            "base64UrlEncode URL-safe")
