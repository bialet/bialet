var d = Date.new(2024, 2, 29, 10, 30, 45)
var tzDate = Date.new(2024, 2, 29, 10, 30, 45, -3)
var midnight = Date.new(2024, 2, 29)
var iso = d.iso
var toStr = d.toString
var dow = Date.new(2024, 9, 13).dayOfWeek
var doy = Date.new(2024, 12, 31).dayOfYear
var woy = Date.new(2024, 9, 13).weekOfYear
var diff = Date.new(2024, 9, 14).diff(Date.new(2024, 9, 13))
var fmt = Date.new(2024, 9, 13).format("#F")
var fmtTime = Date.new(2024, 9, 13, 8, 5, 3).format("#T")
var fmt12 = Date.new(2024, 9, 13, 15, 45, 30).format("#I:#M #p")
var unix = Date.new(2024, 9, 13).unix

return "tz:" + tzDate.tz.toString +
  "|midnight:" + midnight.toString +
  "|iso:" + iso +
  "|str:" + toStr +
  "|dow:" + dow.toString +
  "|doy:" + doy.toString +
  "|woy:" + woy.toString +
  "|diff:" + diff.toString +
  "|fmt:" + fmt +
  "|fmtTime:" + fmtTime +
  "|fmt12:" + fmt12 +
  "|unix:" + (unix > 0).toString
