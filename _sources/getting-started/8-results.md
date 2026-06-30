# 8. Results

The final step: display vote counts with percentage bars.

## Add calculation methods

Extend the `Poll` class in `_app.wren`:

```wren
class Poll {
  construct new() {
    _opts = null
  }

  // Cache query results so we don't re-query on every access
  options { _opts || (_opts = `SELECT * FROM poll`.fetch) }

  vote(opt) {
    `UPDATE poll SET votes = votes + 1 WHERE id = ?`.query(opt)
    _opts = null  // invalidate cache after voting
  }

  totalVotes {
    options.reduce(0, Fn.new {|sum, opt| sum + votes_(opt)})
  }

  percentage(opt) {
    totalVotes > 0 ? ((votes_(opt) / totalVotes) * 100).round : 0
  }

  // Private helper — underscore suffix is a Wren convention
  votes_(opt) { Num.fromString(opt["votes"]) }
}
```

Important details:
- **Caching**: `_opts || (_opts = ...)` fetches once, reuses the result.
  We clear the cache after voting so the results refresh.
- **DB values are strings**: `opt["votes"]` is a string. Use
  `Num.fromString()` before doing math.
- **`.round`**: A property on Number — rounds to the nearest integer.

## Build the results page

`results.wren`:

```wren
import "_app" for Template, Poll

var poll = Poll.new()

Response.out(Template.layout(
<main>
  <h2>Has web development become overly complex?</h2>
  {{ poll.options.map {|opt| <section>
    <h3>{{ opt["answer"] }}</h3>
    <div style="background:#e5e7eb;height:2rem;border-radius:999px">
      <div style="background:#3b82f6;height:2rem;width:{{
        poll.percentage(opt) }}%;border-radius:999px;text-align:center;color:#fff">
        {{ poll.percentage(opt) }}%
      </div>
    </div>
    <p>Total votes: {{ opt["votes"] }}</p>
  </section> } }}
  <p><a href="/">Vote again</a></p>
</main>
))
```

## Final polish

A few improvements we can make:

```wren
// Add color to poll options via migrations
Db.migrate("Add color column", `
  ALTER TABLE poll ADD COLUMN color TEXT
`)

Db.migrate("Set colors", `
  UPDATE poll SET color = "blue" WHERE answer = "Yes";
  UPDATE poll SET color = "red" WHERE answer = "No";
`)
```

Then reference `opt["color"]` in the progress bar style.

## Done!

Your poll app is complete. Here are the final files:

- [\_app.wren](3-final/_app.wren)
- [\_migration.wren](3-final/_migration.wren)
- [index.wren](3-final/index.wren)
- [results.wren](3-final/results.wren)

[See the live demo](https://poll.bialet.dev/).

## Next steps

- Learn more about [Database](../database.md) — ordering, pagination, `.to()`
  mapping
- Explore the [API Reference](../reference.md) for all built-in classes
- Check the [Examples](../examples.rst) page for more patterns
- Read [Structure & Routing](../structure.md) for file-based routing and
  external imports

---
**Previous:** [7. Forms & Voting](7-forms-voting)
