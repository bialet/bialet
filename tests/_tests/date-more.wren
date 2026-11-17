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

// A zone belongs to one Date value. It used to stay in the process environment,
// so one request's Date.new(x, Request.post("tz")) moved the clock for every
// later request and an empty zone could not undo it.
var beforeZone = Date.fromUnix(0).toString
Test.assert(Date.new("2024-01-01 12:00:00", "UTC-9").unix == 1704078000, "string zone applies")
Test.assert(Date.fromUnix(0).toString == beforeZone, "a zone does not leak to later dates")
