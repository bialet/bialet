# Date and Time Functions

This page demonstrates the usage of the `Date` class. It covers basic date and time manipulations, formatting, and how to handle date inputs in various contexts.

See more details in the [Date class reference](date-reference).

## Creating a Date Object

You can create a `Date` object in several ways, representing both the current and specific dates:

```wren
var now = Date.now
var specificDate = Date.new(2024, 9, 13, 15, 45, 30)
var justDate = Date.new(2024, 9, 13)
```

- `Date.now` creates a new `Date` object representing the current date and time.
- `Date.new(year, month, day, hour, minute, second)` allows you to create a specific date and time.
- `Date.new(year, month, day)` creates a date object with the specified year, month, and day at midnight.

## Global UTC Handling

You can set the global UTC offset for all date objects:

```wren
Date.utc = -3 // Set global UTC to GMT-3 (Argentina)
var dateInUtc = Date.now
System.print("Current Date in UTC-3: %(dateInUtc)")
```

This sets the global UTC offset for all subsequent date manipulations.

## Per-Instance UTC Handling

You can also set the UTC offset for individual `Date` instances:

```wren
var date = Date.new(2024, 9, 13, 15, 45, 30)
date.utc = -5 // Set UTC offset for this specific instance to GMT-5
System.print("Date with specific UTC: %(date)")
```

This lets you control the UTC offset at the instance level without affecting other `Date` objects.

## Displaying Date and Time Components

You can access various components of a `Date` object:

```wren
var date = Date.now
System.print("Year: %(date.year)")
System.print("Month: %(date.month)")
System.print("Day: %(date.day)")
System.print("Hour: %(date.hour)")
System.print("Minute: %(date.minute)")
System.print("Second: %(date.second)")
System.print("Unix Timestamp: %(date.unix)")
```

## Adding and Subtracting Dates

You can easily add or subtract time from a `Date` object:

```wren
var date = Date.new(2024, 9, 13)
var newDate = date + 86400 // Add one day (86400 seconds)
System.print("Date after adding a day: %(newDate)")

var previousDate = date - 3600 // Subtract one hour (3600 seconds)
System.print("Date after subtracting an hour: %(previousDate)")
```

## Comparing Dates

Date objects can be compared using standard operators:

```wren
var date1 = Date.new(2024, 9, 13)
var date2 = Date.now

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

| Format Code | Description                                                                 |
|-------------|-----------------------------------------------------------------------------|
| `#d`        | Day of the month with leading zero: 01-31                                   |
| `#e`        | Day of the month without leading zero: 1-31                                 |
| `#f`        | Fractional seconds with millisecond precision: SS.SSS                       |
| `#F`        | ISO 8601 full date format: YYYY-MM-DD                                       |
| `#G`        | ISO 8601 year that corresponds to the ISO week number `#V`                  |
| `#g`        | Two-digit ISO 8601 year corresponding to the ISO week number `#V`           |
| `#H`        | Hour in 24-hour format with leading zero: 00-24                             |
| `#I`        | Hour in 12-hour clock format with leading zero: 01-12                       |
| `#j`        | Day of the year with leading zero: 001-366                                  |
| `#J`        | Julian day number (fractional)                                              |
| `#k`        | Hour in 24-hour format without leading zero: 0-24                           |
| `#l`        | Hour in 12-hour clock format without leading zero: 1-12                     |
| `#m`        | Month with leading zero: 01-12                                              |
| `#M`        | Minute with leading zero: 00-59                                             |
| `#p`        | "AM" or "PM" depending on the hour                                          |
| `#P`        | "am" or "pm" depending on the hour                                          |
| `#R`        | ISO 8601 time format: HH:MM                                                 |
| `#s`        | Seconds since Unix epoch (1970-01-01)                                       |
| `#S`        | Seconds with leading zero: 00-59                                            |
| `#T`        | ISO 8601 full time format: HH:MM:SS                                         |
| `#U`        | Week of the year (00-53), with week 01 starting on the first Sunday         |
| `#u`        | Day of the week (1-7), with Monday as 1 and Sunday as 7                     |
| `#V`        | ISO 8601 week number of the year (01-53)                                    |
| `#w`        | Day of the week (0-6), with Sunday as 0 and Saturday as 6                   |
| `#W`        | Week of the year (00-53), with week 01 starting on the first Monday         |
| `#Y`        | Year with century, including leading zeros: 0000-9999                       |
