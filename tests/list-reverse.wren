// List.reverse returns a new list; the original is untouched.
var xs = [1, 2, 3]
var reversed = xs.reverse
var called = [4, 5].reverse()
var empty = [].reverse

return "rev:" + reversed.join(",") +
  "|call:" + called.join(",") +
  "|empty:" + empty.count.toString +
  "|orig:" + xs.join(",")
