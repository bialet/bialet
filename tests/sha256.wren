var checks = []
checks.add(Util.sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")
checks.add(Util.sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
checks.add(Util.sha256Base64Url("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk") == "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM")
checks.add(Util.encodeBase64("abc") == "YWJj")
checks.add(Util.encodeBase64("a") == "YQ==")
checks.add(Util.encodeBase64("ab") == "YWI=")
checks.add(Util.encodeBase64("") == "")
checks.add(Util.decodeBase64(Util.encodeBase64("héllo ✓")) == "héllo ✓")
checks.add(Util.base64UrlEncode("a") == "YQ")

if (checks.contains(false)) {
  return "failed:" + checks.join(",")
}
return "all-passed"
