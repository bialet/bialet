# Configuration

Bialet stores configuration in the SQLite database — every setting is a row
in a simple key/value table. There are no `.env` files, no YAML, no
environment variables to set.

## Reading and Writing

```wren
Config.set("app_name", "My App")
var name = Config.get("app_name")  // "My App"
```

### Typed Access

All values are stored as strings. Use typed getters for numbers, booleans,
and JSON:

```wren
Config.set("items_per_page", "20")
var limit = Config.num("items_per_page")  // 20 as a number

Config.set("maintenance_mode", "1")
var on = Config.bool("maintenance_mode")  // true

Config.json("features", {"darkMode": true, "notifications": false})
var features = Config.json("features")     // { "darkMode": true, ... }
```

`Config.bool` returns `true` for `"1"` and `"true"` (case-insensitive),
`false` for everything else including missing keys.

### Enable / Disable

For on/off flags, use the `enable` and `disable` convenience methods
instead of hand-writing the `"1"`/`"0"` strings:

```wren
Config.enable("beta_features")   // stores "1"
Config.disable("beta_features")  // stores "0"
Config.bool("beta_features")     // false

if (Config.bool("live_reload")) {
  // feature is on
}
```

Bialet reads a few flags itself at startup. `BIALET_LIVE_RELOAD` enables the
live-reload script (see [Live Reload](live-reload.md)), and
`BIALET_SHOW_ERRORS` shows compile/runtime errors in the browser instead of the
generic 500 page (see [Error Pages](errors.md)).

### Development mode: `bialet dev`

The `dev` subcommand is the fastest way to start developing:

```bash
bialet dev
```

It starts the server from the current directory and:

- Enables `BIALET_LIVE_RELOAD` and `BIALET_SHOW_ERRORS` in the database. This
  is idempotent — run it once and the flags stay on for later `bialet` runs; if
  they were disabled, `bialet dev` turns them back on.
- Opens `http://127.0.0.1:7001` (or your `-p` port) in the default browser,
  like `react-scripts start`.

Plain `bialet` starts the server without touching the flags or opening the
browser.

`enable` sets the value to `"1"`, `disable` sets it to `"0"`. Both are
equivalent to `Config.set("key", "1")` and `Config.set("key", "0")`.

### Delete

```wren
Config.delete("old_setting")
```

> ⚠️ Pitfall: `Config.get()` returns `null` for missing keys. Always
> provide a fallback: `Config.get("title") || "Default Title"`.

## How It Works

Under the hood, `Config` reads and writes to the `BIALET_CONFIG` table, a
two-column table created automatically when the database initializes:

```sql
CREATE TABLE IF NOT EXISTS BIALET_CONFIG (
  key   TEXT PRIMARY KEY,
  value TEXT
)
```

`Config.set("key", "value")` runs an `INSERT OR REPLACE`. `Config.get("key")`
does a `SELECT value FROM BIALET_CONFIG WHERE key = ?`. `.num`, `.bool`, and
`.json` convert the string result into the right type in Wren — the database
still stores everything as `TEXT`.

Because configuration is just rows in SQLite, changes persist across server
restarts with no extra work. There's no file to parse, no cache to
invalidate, and no separate config server to run.

## Seeding Configuration

### With `-r` at startup

The `-r` flag runs Wren code before the server starts and then exits. Use
it in deployment scripts to seed secrets and settings:

```bash
bialet -r 'Config.set("stripe_key", "sk_live_...")' /www/myapp
bialet -r 'Config.set("mail_host", "smtp.example.com")' /www/myapp
```

`-r` can run any Wren code, so you can set multiple values in one call or
even build a setup script:

```wren
// setup.wren — run with: bialet -r "$(cat setup.wren)" /www/myapp
Config.set("title", "My App")
Config.set("items_per_page", "20")
Config.json("features", {"darkMode": true, "notifications": false})
```

> Use `-r` for one-off provisioning — secrets, API keys, and
> environment-specific overrides that shouldn't live in migrations.

### With migrations for defaults

For defaults that ship with the application and should exist in every
environment, write them in a migration:

```wren
// _migration.wren
Db.migrate("Add default config", [
  `INSERT OR IGNORE INTO BIALET_CONFIG VALUES ('title', 'My App')`,
  `INSERT OR IGNORE INTO BIALET_CONFIG VALUES ('items_per_page', '20')`,
  `INSERT OR IGNORE INTO BIALET_CONFIG VALUES ('admin_email', 'admin@example.com')`
])
```

`INSERT OR IGNORE` sets a fallback value — it only inserts if the key
doesn't already exist. This lets `-r` overrides from deployment scripts
take precedence over migration defaults.

## Working with Environments

Bialet has no `development`, `staging`, or `production` mode. Each
environment is a different database file with its own `BIALET_CONFIG`
table. The database *is* the environment.

```text
dev/
├── _db.sqlite3          # Dev config
├── _migration.wren
└── index.wren

prod/
├── _db.sqlite3          # Prod config
├── _migration.wren
└── index.wren
```

Migrations and application code are identical across environments — only
the database file differs, and therefore only the configuration and data
differ. Pointing Bialet at a different `_db.sqlite3` switches environments.

## Comparison with Other Frameworks

Most frameworks layer configuration from multiple sources: `.env` files,
environment variables, YAML configs, command-line flags, and sometimes a
configuration database. They merge them with precedence rules (env vars
beat `.env`, flags beat env vars, etc.), and you need to know which source
wins in which context.

Bialet removes that entire stack. There is one source: the database.

| | Bialet | Rails / Django / Laravel | Express / Next.js |
|---|---|---|---|
| Config storage | SQLite rows | `.env` + YAML/rb/py + ENV | `.env` + ENV |
| Precedence rules | None | Multi-layer (env beats file, etc.) | Multi-layer |
| Persists across restarts | Yes (it's a DB row) | No (env vars are per-process) | No |
| Programmatic write | `Config.set()` | Usually read-only from code | Usually read-only |
| Environment switching | Point to a different `_db.sqlite3` | `RAILS_ENV`, `NODE_ENV`, etc. | `NODE_ENV` |
| Configuration UI | Possible — just writes to SQLite | Needs a backing store | Needs a backing store |
| Secrets at deploy time | `bialet -r 'Config.set(...)'` | `.env` file or CI env vars | `.env` file or CI env vars |

The trade-off: you can't configure Bialet through environment variables or
`.env` files. If your deployment pipeline relies on those, you'll need to
translate them into `-r` calls or migration defaults. The benefit: no
precedence bugs, no runtime parsing overhead, and a single table to query
for every setting in the system.

## Common Patterns

### Feature Flags

```wren
if (Config.bool("beta_features")) {
  // Show beta UI
}
```

Toggle flags with `Config.enable()` / `Config.disable()`:

```wren
Config.enable("beta_features")   // turn on
Config.disable("beta_features")  // turn off
```

### Maintenance Mode

```wren
if (Config.bool("maintenance_mode")) {
  Response.status(503)
  return <main><h1>We'll be right back</h1></main>
}
```

### Pagination Settings

```wren
var limit = Config.num("items_per_page") || 20
var page = Num.fromString(Request.get("page") || "1")
var offset = (page - 1) * limit
```

### Site-Wide Title

```wren
var title = Config.get("site_title") || "My Site"
```

## Pitfalls

- **`Config.get()` returns `null` for missing keys** — always provide a
  fallback: `Config.get("key") || "default"`.
- **Config values are strings** — use `Config.num()` and `Config.bool()`
  for typed access. `Config.get("port")` returns `"7001"`, not `7001`.
- **No `.env` files or environment variables** — everything lives in
  `BIALET_CONFIG`. If you need inject values from outside, use `-r`.
- **`-r` runs once and exits** — it's for provisioning, not for setting
  values that the server reads on every request. Those belong in migrations
  or are set once via `-r` and then read via `Config.get()` at runtime.
- **Keep your production database out of version control** — it contains
  your config, user data, and sessions.

## Next Steps

- Learn about [Database](database.md) — the Query object, migrations, and
  how `BIALET_CONFIG` sits alongside your application tables
- Set up a production environment with [Deployment](deployment.md)
- See the [Security](security.md) guide for CSRF, headers, and secure
  configuration
- Check the [Reference](reference.md) for the full `Config` API
