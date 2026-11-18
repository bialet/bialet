// Date.new(number) is epoch seconds, the same unit as .unix; fromUnix is the
// other constructor for the same value. A round trip must preserve the local
// date, in the default zone and in a named one.
var d = Date.new(2024, 9, 13, 10, 30, 45)
var round = Date.fromUnix(d.unix)
var fromNumber = Date.new(d.unix)
var fromNumberTz = Date.new(d.unix, "UTC")

return "round:" + round.toString +
  "|eq:" + (round == d).toString +
  "|zero:" + (Date.fromUnix(0) is Date).toString +
  "|num:" + fromNumber.toString +
  "|numTz:" + fromNumberTz.tz +
  "|numEq:" + (fromNumber == d).toString
