// The native split/trim/safe/secureEquals primitives keep the edge cases of
// the Wren implementations they replaced.
var parts = "a,,b,".split(",")
var empty = "".split(",")
var one = "a".split(",")
var dangerous = "<\"&'>"

return "split:" + parts.join("|") +
  "|empty:" + empty.count.toString + ":" + (empty[0] == "").toString +
  "|one:" + one.join("|") +
  "|trim:[" + "  x \n".trim() + "]" +
  "|safe:" + dangerous.safe +
  "|eq:" + Util.secureEquals("abc", "abc").toString +
  Util.secureEquals("abc", "abd").toString +
  Util.secureEquals("abc", "ab").toString
