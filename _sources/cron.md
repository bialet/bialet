# Cron Tasks

Bialet allows you to run scheduled code by defining a cron job in a Wren file.

This is useful for running periodic operations like cleaning old records, sending emails, or syncing external data.

The cron file can be in the root and be called `_cron.wren` or be inside the `_app` folder, `_app/cron.wren`.

## Basic Usage

To schedule a task, use the `Cron.every` or `Cron.at` helpers methods inside the Wren cron file.
These methods receive a block that is called at the specified time.

```wren
import "bialet" for Cron
import "/_domain" for Task

// Every 2 minutes
Cron.every(2) { |date| "Hello, from Cron!" }

// At 2:00 AM
Cron.at(2, 0) { |date| Task.clearAll() }
```

The `|date|` argument is the current [Date object](datetime) used for evaluation. You can ignore it if not needed.

### Helper methods

#### `Cron.every(minutes)`

Runs the job when the current minute is divisible by `minutes`.

```wren
Cron.every(10) { |d| "Runs every 10 minutes" }
```
#### `Cron.at(hour, minute, dayOfWeek)`

Runs the job when the current time matches the given `hour` and `minute`.

```wren
Cron.at(3, 15) { |d| "Runs at 03:15 every day" }
```

Runs the job when the current time matches the given hour, minute, and day of week.
Days of week: 0 (Sunday) to 6 (Saturday)

```wren
Cron.at(4, 30, 1) { |d| "Runs at 04:30 every Monday" }
```

## Custom Rules

Each job receives a `Date` object of the current time, which can be used for custom rules.

```wren
Cron.every(1) { |d|
  if (d.day == 1 && d.hour == 0) {
    "It's the first of the month!"
  }
}
```
