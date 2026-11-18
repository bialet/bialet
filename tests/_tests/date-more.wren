Test.assert(Date.new(2024, 2, 29).toString == "2024-02-29 00:00:00", "Date 3-arg constructor")
Test.assert(Date.new(2024, 2, 29, 10, 30, 45).iso == "2024-02-29 10:30:45", "Date iso")
Test.assert(Date.new(2024, 2, 29, 10, 30, 45, -3).tz == -3, "Date tz constructor")
Test.assert(Date.new(2024, 9, 13).dayOfWeek == 5, "Date dayOfWeek")
Test.assert(Date.new(2024, 12, 31).dayOfYear == 366, "Date dayOfYear")
Test.assert(Date.new(2024, 9, 13).weekOfYear == 37, "Date weekOfYear")
Test.assert(Date.new(2024, 9, 14).diff(Date.new(2024, 9, 13)) == 86400, "Date diff")
Test.assert(Date.new(2024, 9, 13).format("#F") == "2024-09-13", "Date format date")
Test.assert(Date.new(2024, 9, 13, 8, 5, 3).format("#T") == "08:05:03", "Date format time")
Test.assert(Date.new(2024, 9, 13, 15, 45, 30).format("#I:#M #p") == "03:45 PM", "Date format 12-hour")
Test.assert(Date.new(2024, 9, 13).unix > 0, "Date unix")
Test.assert(Date.new(2024, 9, 13) < Date.new(2024, 9, 14), "Date comparison")

// tz= works, and a numeric offset no longer makes the native primitives throw
// ("Timezone must be a string."): the docs show `date.tz = -5`.
var zoned = Date.new(2024, 9, 13, 15, 45, 30, 0)
Test.assert(zoned.unix > 0, "numeric tz constructor reaches unix")
zoned.tz = -5
Test.assert(zoned.tz == -5, "Date.tz setter")
Test.assert(zoned.hours == 15, "Date.tz setter keeps the wall clock")
Test.assert(zoned.toString == "2024-09-13 15:45:30", "numeric tz keeps toString")
Test.assert(zoned.format("#F") == "2024-09-13", "numeric tz reaches format")

// A numeric tz is the UTC offset the docs describe (-3 is UTC-3). It used to be
// handed to TZ unchanged, where "-3" means UTC+3, so every numeric offset was
// applied with the wrong sign.
Test.assert(Date.new(2024, 1, 1, 12, 0, 0, -3).unix == 1704121200, "tz -3 is UTC-3")
Test.assert(Date.new(2024, 1, 1, 12, 0, 0, 3).unix == 1704099600, "tz 3 is UTC+3")
Test.assert(Date.new(2024, 1, 1, 12, 0, 0, 5.5).unix == 1704090600, "tz 5.5 is UTC+5:30")
Test.assert(Date.new(2024, 1, 1, 12, 0, 0, 0).unix == 1704110400, "tz 0 is UTC")

// A zone belongs to one Date value. It used to stay in the process environment,
// so one request's Date.new(x, Request.post("tz")) moved the clock for every
// later request and an empty zone could not undo it.
var beforeZone = Date.fromUnix(0).toString
Test.assert(Date.new("2024-01-01 12:00:00", "UTC-9").unix == 1704078000, "string zone applies")
Test.assert(Date.fromUnix(0).toString == beforeZone, "a zone does not leak to later dates")

// A zone string reaches TZ, where ":path" names a zone file to open; only
// zone names and UTC offsets are accepted.
var badZone = Fiber.new { Date.new("2024-01-01 12:00:00", ":/etc/passwd").unix }
badZone.try()
Test.assert(badZone.error != null, "a ':path' zone is refused")
var dotZone = Fiber.new { Date.new("2024-01-01 12:00:00", "../../etc/x").unix }
dotZone.try()
Test.assert(dotZone.error != null, "a relative path zone is refused")
Test.assert(Date.new("2024-01-01 12:00:00", "Etc/GMT+3").unix == 1704121200, "zone names still work")

// Date.new(number) is epoch seconds, the same unit as .unix, in Date.tz unless
// a zone is given. fromUnix() keeps its meaning and gains the same overload.
var stamp = 1700000000
Test.assert(Date.new(stamp, "UTC").toString == "2023-11-14 22:13:20", "Date.new(seconds) reads epoch seconds")
Test.assert(Date.fromUnix(stamp, "UTC").toString == "2023-11-14 22:13:20", "Date.fromUnix(seconds, tz)")
Test.assert(Date.new(stamp, "UTC").tz == "UTC", "Date.new(seconds, tz) keeps the zone")
var roundTrip = Date.new(2024, 9, 13, 10, 30, 45)
Test.assert(Date.new(roundTrip.unix) == roundTrip, "Date.new(.unix) round-trips")

// `+` and `-` with a number shift the instant by seconds; two dates subtract
// to the seconds between them, like .diff.
var base = Date.new(2024, 1, 1, 0, 0, 0)
Test.assert((base + 86400) == Date.new(2024, 1, 2), "Date + seconds")
Test.assert((base + 90).unix == base.unix + 90, "Date + adds exactly the seconds")
Test.assert((Date.new(2024, 1, 2) - 86400) == base, "Date - seconds")
Test.assert(Date.new(2024, 1, 2) - base == 86400, "Date - Date is seconds")
Test.assert((base + 3600).hours == 1, "Date + seconds keeps the wall clock")

// Adding two instants has no meaning, and a non-number is not a duration.
var addDates = Fiber.new { base + base }
addDates.try()
Test.assert(addDates.error != null && addDates.error.contains("undefined"), "Date + Date aborts")
var addText = Fiber.new { base + "1" }
addText.try()
Test.assert(addText.error != null && addText.error.contains("number of seconds"), "Date + non-number aborts")
var subText = Fiber.new { base - "1" }
subText.try()
Test.assert(subText.error != null && subText.error.contains("number of seconds or a Date"), "Date - non-number aborts")

// addDays() shifts the civil date, so month ends and leap years come out right
// and the wall clock is preserved.
Test.assert(Date.new(2024, 2, 28).addDays(1).toString == "2024-02-29 00:00:00", "addDays crosses a leap day")
Test.assert(Date.new(2023, 2, 28).addDays(1).toString == "2023-03-01 00:00:00", "addDays after a non-leap February")
Test.assert(Date.new(2024, 1, 31).addDays(1) == Date.new(2024, 2, 1), "addDays at a month end")
Test.assert(Date.new(2024, 5, 31).addDays(-31) == Date.new(2024, 4, 30), "addDays clamps to a shorter month")
Test.assert(Date.new(2023, 12, 31).addDays(1) == Date.new(2024, 1, 1), "addDays across a year boundary")
Test.assert(Date.new(2024, 3, 1).addDays(-1) == Date.new(2024, 2, 29), "addDays with a negative day")
Test.assert(Date.new(2024, 2, 28, 23, 59, 59).addDays(1).toString == "2024-02-29 23:59:59", "addDays keeps the time")
Test.assert(Date.new(2024, 2, 28).addDays(0) == Date.new(2024, 2, 28), "addDays(0) is the same date")

// A zone survives the shift, and a DST change moves the instant by 23 or 25
// hours while 09:00 stays 09:00.
var ny = Date.new(2024, 3, 9, 9, 0, 0, "America/New_York")
var nyNext = ny.addDays(1)
Test.assert(nyNext.tz == "America/New_York", "addDays keeps the zone")
Test.assert(nyNext.toString == "2024-03-10 09:00:00", "addDays keeps the wall clock over DST")
Test.assert(nyNext.unix - ny.unix == 82800, "addDays crosses spring forward in 23 hours")
var nyFall = Date.new(2024, 11, 2, 9, 0, 0, "America/New_York")
Test.assert(nyFall.addDays(1).unix - nyFall.unix == 90000, "addDays crosses fall back in 25 hours")

var fraction = Fiber.new { base.addDays(1.5) }
fraction.try()
Test.assert(fraction.error != null && fraction.error.contains("whole number"), "addDays rejects a fraction")
var dayText = Fiber.new { base.addDays("1") }
dayText.try()
Test.assert(dayText.error != null && dayText.error.contains("whole number"), "addDays rejects text")

