# Date and Time Functions

This page demonstrates the usage of the `Date` class. It covers basic date and
time manipulations, formatting, and how to handle date inputs in various
contexts.

See more details in the [Date class reference](date-reference).

## Creating a Date Object

You can create a `Date` object in several ways, representing both the current
and specific dates:

```wren
var now = Date.new()
var specificDate = Date.new(2024, 9, 13, 15, 45, 30)
var justDate = Date.new(2024, 9, 13)
var fromEpoch = Date.fromUnix(1700000000)
var fromEpochNumber = Date.new(1700000000)
```

- `Date.new()` creates a new `Date` object representing the current date and time.
- `Date.now` is a static getter equivalent to `Date.new()`, returning the current date and time.
- `Date.new(year, month, day, hour, minute, second)` allows you to create a
  specific date and time.
- `Date.new(year, month, day)` creates a date object with the specified year,
  month, and day at midnight.
- `Date.new(seconds)` and `Date.fromUnix(seconds)` build the instant
  `seconds` after 1970, formatted in `Date.tz`. Add a zone as the second
  argument: `Date.new(1700000000, "UTC")`.

## Per-Instance Timezone Handling

You can set the timezone offset for individual `Date` instances:

```wren
var date = Date.new(2024, 9, 13, 15, 45, 30, -3)
System.print("Date with timezone: %(date)")
```

Or set it after creation:

```wren
var date = Date.new(2024, 9, 13, 15, 45, 30)
date.tz = -5 // Set timezone offset for this specific instance to GMT-5
System.print("Date with specific timezone: %(date)")
```

This lets you control the UTC offset at the instance level without affecting
other `Date` objects.

A zone is either a number (hours from UTC, `-3` is UTC-3) or a zone name such
as `"America/Bogota"` or `"Etc/GMT+3"`. Anything else — a path, a string with
`:` or `.` — raises an error, so a zone taken from a form field cannot make the
server read a file. Prefer an allow-list of zones your app offers anyway.

> ⚠️ Pitfall: string zones keep POSIX signs, which count hours **west** of UTC:
> `"UTC+3"` and `"Etc/GMT+3"` are UTC-3, while the **number** `3` is UTC+3.
> Prefer an `"Area/City"` name; the numeric form has no such ambiguity.

## Application Timezone

`BIALET_TIMEZONE` sets the zone every `Date` uses when the value does not carry
one of its own. It applies to migrations, cron jobs and every request:

```wren
Config.set("BIALET_TIMEZONE", "America/Bogota")
```

```wren
Date.new()                // now, in America/Bogota
Date.fromUnix(1700000000) // 2023-11-14 17:13:20
Date.tz                   // "America/Bogota"
```

Without the row, dates follow the process zone: the `TZ` of the environment
that started the server (UTC on most servers).

> ⚠️ Pitfall: the value is read once, when the server starts. Setting it from a
> request handler, or editing the row while the server runs, has no effect until
> you restart.

- A value the zone rules refuse (a path, a POSIX rule string) is ignored with a
  `BIALET_TIMEZONE is not a zone name or UTC offset; ignoring it` line at
  startup, and the process zone is used instead.
- A zone name needs the operating system's timezone database. Windows does not
  ship one, so there use `"UTC"` or a `"UTC<offset>"` string, with the same
  west-positive sign as above; `"Area/City"` names only work on Linux and macOS.
- It does **not** change what SQLite stores: `CURRENT_TIMESTAMP`, `date('now')`
  and a `DEFAULT CURRENT_TIMESTAMP` column stay UTC. It does set the zone of
  SQLite's `localtime` modifier and the `YYYY-MM-DD HH:MM:SS` prefix of Bialet's
  own log lines.
- `Date.init(zone)` overrides the default for the current script or request.

### Stored Timestamps

Keep storing UTC. Bialet does not change the timestamps SQLite writes —
`CURRENT_TIMESTAMP`, `date('now')` and a `DEFAULT CURRENT_TIMESTAMP` column are
UTC — and that is what you want: a local timestamp is ambiguous for the hour a
DST change repeats, it stops ordering correctly, and it is wrong the day the app
moves to another zone. Retention (`Db.clean`, the session sweep, the temporary
upload purge) compares against UTC too.

`BIALET_TIMEZONE` applies to SQLite's explicit `localtime` modifier, which is the
conversion to the app zone:

```wren
var rows = `SELECT datetime(createdAt, 'localtime') AS createdAt,
                   date('now', 'localtime') AS today
            FROM items`.fetch()
```

Reading a stored UTC value as a `Date` in the app zone:

```wren
var stored = row["createdAt"]                // "2023-11-14 22:13:20", UTC
var date = Date.fromUnix(Date.new(stored, "UTC").unix)
System.print(date)                            // 2023-11-14 17:13:20, America/Bogota
```

> ⚠️ Pitfall: a `Date` saved through `Query.save` or as a query parameter is
> written as its wall-clock string, with no offset. `Date.now` therefore stores
> the app zone's wall clock, which is *not* UTC. Store `Date.now.unix` (or a
> `CURRENT_TIMESTAMP` default) when you need an unambiguous instant.

The `createdAt` column of `BIALET_LOGS` is UTC like every other stored timestamp,
even though the line Bialet prints to the log has a prefix in the app zone.

## Displaying Date and Time Components

You can access various components of a `Date` object:

```wren
var date = Date.new()
System.print("Year: %(date.year)")
System.print("Month: %(date.month)")
System.print("Day: %(date.day)")
System.print("Hours: %(date.hours)")
System.print("Minutes: %(date.minutes)")
System.print("Seconds: %(date.seconds)")
System.print("Unix Timestamp: %(date.unix)")
```

> ⚠️ Pitfall: the time-of-day getters are **plural** — `.hours`, `.minutes`,
> `.seconds` — unlike `.year`, `.month`, and `.day`, which are singular.
> `date.hour` raises `Date does not implement 'hour'.`.

## Adding and Subtracting Time

`Date` works in two units. `+` and `-` with a **number** shift the instant by
**seconds**, the same unit as `.unix`; subtracting one `Date` from another
returns the seconds between them:

```wren
var start = Date.new("2024-09-13 10:00:00")
var end = start + 9000
System.print(end)               // 2024-09-13 12:30:00
System.print(end - start)       // 9000
System.print(end.diff(start))   // 9000
```

Seconds are exact: across a DST change a "day" of 86400 seconds can land on a
different wall clock, which is what you want when you mean an elapsed duration.

`date + otherDate` is refused: adding two instants has no meaning. Subtract
them instead.

For **calendar days** use `addDays(n)`, which shifts the year/month/day fields
and keeps the hours, minutes, seconds and zone:

```wren
Date.new(2024, 2, 28).addDays(1)              // 2024-02-29 00:00:00
Date.new(2023, 2, 28).addDays(1)              // 2023-03-01 00:00:00
Date.new(2024, 1, 31).addDays(1)              // 2024-02-01 00:00:00
Date.new(2024, 3, 9, 9, 0, 0, "America/New_York").addDays(1).toString
  // 2024-03-10 09:00:00: 09:00 stays 09:00, and .unix moves 23 hours
```

`addDays` requires a whole number (negative is allowed). Because it never
touches the clock fields it is month-end, leap-year and DST safe: `09:00` stays
`09:00` even when the day is 23 or 25 hours long. Month arithmetic is not built
in; clamp the day yourself (see
[Dates in Recipes](recipes.md#dates-add-days-add-months-week-start-due-dates)).

`Date.new(number)` and `Date.fromUnix(number)` both read **epoch seconds**;
adding a zone (`Date.new(1700000000, "UTC")`) formats the same instant there.

## Comparing Dates

Date objects can be compared using standard operators:

```wren
var date1 = Date.new(2024, 9, 13)
var date2 = Date.new()

System.print(date1 > date2 ? "Date1 is in the future" : "Date1 is not in the future")
System.print(date1 < date2 ? "Date1 is in the past" : "Date1 is not in the past")
System.print(date1 == date2 ? "Dates are equal" : "Dates are not equal")
```

## Formatting Dates

You can format dates using the `format()` method:

```wren
var date = Date.new(2024, 9, 13)
System.print("Formatted Date: %(date.format('#d/#m/#Y'))") // Output: 13/09/2024
```

Refer to the table below for available format options.

## Formatting Patterns

| Format Code | Description                                                         |
| ----------- | ------------------------------------------------------------------- |
| `#d`        | Day of the month with leading zero: 01-31                           |
| `#e`        | Day of the month without leading zero: 1-31                         |
| `#f`        | Fractional seconds with millisecond precision: SS.SSS               |
| `#F`        | ISO 8601 full date format: YYYY-MM-DD                               |
| `#G`        | ISO 8601 year that corresponds to the ISO week number `#V`          |
| `#g`        | Two-digit ISO 8601 year corresponding to the ISO week number `#V`   |
| `#H`        | Hour in 24-hour format with leading zero: 00-23                     |
| `#I`        | Hour in 12-hour clock format with leading zero: 01-12               |
| `#j`        | Day of the year with leading zero: 001-366                          |
| `#J`        | Julian day number (fractional)                                      |
| `#k`        | Hour in 24-hour format without leading zero: 0-23                   |
| `#l`        | Hour in 12-hour clock format without leading zero: 1-12             |
| `#m`        | Month with leading zero: 01-12                                      |
| `#M`        | Minute with leading zero: 00-59                                     |
| `#p`        | "AM" or "PM" depending on the hour                                  |
| `#P`        | "am" or "pm" depending on the hour                                  |
| `#R`        | ISO 8601 time format: HH:MM                                         |
| `#s`        | Seconds since Unix epoch (1970-01-01)                               |
| `#S`        | Seconds with leading zero: 00-59                                    |
| `#T`        | ISO 8601 full time format: HH:MM:SS                                 |
| `#U`        | Week of the year (00-53), with week 01 starting on the first Sunday |
| `#u`        | Day of the week (1-7), with Monday as 1 and Sunday as 7             |
| `#V`        | ISO 8601 week number of the year (01-53)                            |
| `#w`        | Day of the week (0-6), with Sunday as 0 and Saturday as 6           |
| `#W`        | Week of the year (00-53), with week 01 starting on the first Monday |
| `#Y`        | Year with century, including leading zeros: 0000-9999               |
