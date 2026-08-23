# Cron Tasks

Bialet allows you to run scheduled code by defining a cron job in a Wren file.

This is useful for running periodic operations like cleaning old records,
sending emails, or syncing external data.

The cron file can be in the root and be called `_cron.wren` or be inside the
`_app` folder, `_app/cron.wren`.

## Basic Usage

To schedule a task, use the `Cron.every` or `Cron.at` helpers methods inside the
Wren cron file. These methods receive a block that is called at the specified
time.

```wren
import "/_app/domain" for Task

// Every 2 minutes
Cron.every(2) { |date| System.log("Hello, from Cron!") }

// At 2:00 AM
Cron.at(2, 0) { |date| Task.clearAll() }
```

The `|date|` argument is the current [Date object](datetime) used for
evaluation. You can ignore it if not needed.

### Helper methods

#### `Cron.every(minutes)`

Runs the job when the current minute is divisible by `minutes`.

```wren
Cron.every(10) { |d| System.log("Runs every 10 minutes") }
```

> **Important:** `Cron.every(n)` uses divisibility, not elapsed time.
> `Cron.every(7)` fires at minutes 0, 7, 14, 21, 28, 35, 42, 49, 56,
> then skips 4 minutes to 0 again. `Cron.every(90)` **never** fires because
> 90 does not divide 60. Use these safe values: 1, 2, 3, 4, 5, 6, 10, 12,
> 15, 20, 30.

#### `Cron.at(hour, minute, dayOfWeek)`

Runs the job when the current time matches the given `hour` and `minute`.

```wren
Cron.at(3, 15) { |d| System.log("Runs at 03:15 every day") }
```

Runs the job when the current time matches the given hour, minute, and day of
week. Days of week: 0 (Sunday) to 6 (Saturday)

```wren
Cron.at(4, 30, 1) { |d| System.log("Runs at 04:30 every Monday") }
```

## Custom Rules

Each job receives a `Date` object of the current time, which can be used for
custom rules.

```wren
Cron.every(1) { |d|
  if (d.day == 1 && d.hour == 0) {
    System.log("It's the first of the month!")
  }
}
```
