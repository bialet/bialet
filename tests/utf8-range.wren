// Regression: wrenNewStringFromRange must treat a continuation byte as owned
// by the lead byte that encodes it. The earlier fix for truncated sequences
// carried every byte that fails to decode through raw, and a continuation byte
// fails to decode on its own, so each multi-byte character was appended twice
// on every range slice. Request.init and String.split/trim/replace all slice,
// so non-ASCII request values reached the app corrupted.

var HEXDIGITS = "0123456789abcdef"
var hex = Fn.new { |s|
  var out = []
  for (b in s.bytes) {
    out.add(HEXDIGITS[b >> 4] + HEXDIGITS[b & 15])
  }
  return out.join(",")
}

var results = []

// Forward slice over the whole string must be byte-identical to the source.
var cafe = "café"
results.add("lit:" + (cafe[0..-1] == cafe).toString)
results.add("litHex:" + hex.call(cafe[0..-1]))

var emoji = "a😀b"
results.add("emoji:" + (emoji[0..-1] == emoji).toString)
results.add("emojiHex:" + hex.call(emoji[0..-1]))

// Reverse slice visits the continuation before its lead; it must not duplicate
// the byte nor drop the character the pair encodes.
results.add("reverseHex:" + hex.call(cafe[4..0]))
results.add("reverse:" + (cafe[4..0] == "éfac").toString)

// Partial slices that start in the middle of a character are unchanged: a
// continuation byte that belongs to the slice but whose lead is outside of it
// is carried through raw.
results.add("tailHex:" + hex.call(cafe[4..4]))
results.add("midHex:" + hex.call(emoji[1..4]))

// The same slice is reached through split/trim/replace, which every app uses.
results.add("split:" + ("uno,dós,tres".split(",")[1] == "dós").toString)
results.add("splitHex:" + hex.call("uno,dós,tres".split(",")[1]))
results.add("trim:" + ("  señor  ".trim() == "señor").toString)
results.add("trimHex:" + hex.call("  señor  ".trim()))
results.add("replace:" + ("café con leche".replace("con", "sin") == "café sin leche").toString)
results.add("replaceHex:" + hex.call("café con leche".replace("con", "sin")))

// A request value goes through the same path: Request.init slices the query
// string with a range before parseQuery splits it.
var posted = Request.post("v")
results.add("posted:" + (posted == "café").toString)
results.add("postedHex:" + hex.call(posted == null ? "" : posted))

return results.join("|")
