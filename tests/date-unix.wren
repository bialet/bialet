// Date.new(number) is a year, not a timestamp; fromUnix is the epoch
// constructor. A round trip through .unix must preserve the local date.
var d = Date.new(2024, 9, 13, 10, 30, 45)
var round = Date.fromUnix(d.unix)

return "round:" + round.toString +
  "|eq:" + (round == d).toString +
  "|zero:" + (Date.fromUnix(0) is Date).toString
