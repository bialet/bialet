# Configuration

Bialet stores configuration in the SQLite database, not in `.env` files or
YAML. Each environment gets its own database file, so environment-specific
settings follow naturally — the database you point to *is* the environment.

## The BIALET_CONFIG Table

Configuration lives in the `BIALET_CONFIG` table, created automatically by
the database initialization:

```sql
CREATE TABLE IF NOT EXISTS BIALET_CONFIG (
  key   TEXT PRIMARY KEY,
  value TEXT
)
```

Use the `Config` class to read and write values:

```wren
// Read a value
var title = Config.get("title")

// Set a value
Config.set("title", "My Bialet App")

// Delete a value
Config.delete("old_setting")
```

> ⚠️ Pitfall: `Config.get()` returns `null` for missing keys. Always provide
> a fallback: `Config.get("title") || "Default Title"`.

## Reading and Writing Config

### Strings

```wren
Config.set("app_name", "My App")
var name = Config.get("app_name")  // "My App"
```

### Numbers

```wren
Config.set("items_per_page", "20")
var limit = Config.num("items_per_page")  // 20 as a number
```

### Booleans

```wren
Config.set("maintenance_mode", "1")
var maintenance = Config.bool("maintenance_mode")  // true
```

`Config.bool` returns `true` for `"1"` and `"true"` (case-insensitive),
`false` for everything else including missing keys.

### JSON

```wren
// Store a structured value
Config.json("features", {"darkMode": true, "notifications": false})

// Read it back as a map
var features = Config.json("features")
var darkMode = features["darkMode"]  // true
```

## Migration-Set Configuration

Set configuration values in migrations for reliable, version-controlled
defaults:

```wren
// _migration.wren
Db.migrate("Add default config values", [
  `INSERT INTO BIALET_CONFIG VALUES ('title', 'My App')`,
  `INSERT INTO BIALET_CONFIG VALUES ('items_per_page', '20')`,
  `INSERT INTO BIALET_CONFIG VALUES ('admin_email', 'admin@example.com')`
])
```

Use `INSERT OR IGNORE` to set defaults that won't overwrite existing values:

```wren
`INSERT OR IGNORE INTO BIALET_CONFIG VALUES ('title', 'My App')`.query
```

## Environment-Specific Configuration

Bialet has no concept of "environments" — there is no `development`,
`staging`, or `production` mode in the framework. Each environment is
simply a different database file with its own configuration.

```text
dev/
├── _db.sqlite3          # Dev config in BIALET_CONFIG
├── _migration.wren
└── index.wren

prod/
├── _db.sqlite3          # Prod config in BIALET_CONFIG
├── _migration.wren
└── index.wren
```

You can override the database path at startup to point to different
environments:

```bash
# Development
bialet -d dev/_db.sqlite3 dev/

# Production
bialet -d prod/_db.sqlite3 prod/
```

The database file is the only thing separating environments. Migrations and
application code are the same across environments — only the data (and
therefore the config) differs.

## Server CLI Options

Start Bialet with:

```bash
bialet [options] [root_dir]
```

| Flag | Option | Default | Description |
|---|---|---|---|
| `-h` | host | `127.0.0.1` | Host address to bind to |
| `-p` | port | `7001` | Port number (0–65535) |
| `-l` | log | *stdout* | Append log output to a file |
| `-d` | database | `_db.sqlite3` | SQLite database file path |
| `-w` | — | *off* | Enable WAL mode for SQLite |
| `-m` | soft memory | *unlimited* | Soft memory limit in MB |
| `-M` | hard memory | *unlimited* | Hard memory limit in MB |
| `-c` | soft CPU | *unlimited* | Soft CPU limit in percent |
| `-C` | hard CPU | *unlimited* | Hard CPU limit in percent |
| `-i` | ignored | `README*,AGENTS*,LICENSE*,*.json,*.yml,*.yaml` | Glob patterns for ignored files |
| `-v` | — | — | Print version and exit |

The `root_dir` positional argument sets the application directory. Defaults
to the current directory.

### Host and Port

```bash
# Listen on all interfaces, port 8080
bialet -h 0.0.0.0 -p 8080 /www/myapp

# Listen only on localhost (default)
bialet /www/myapp
```

> For production, bind to `127.0.0.1` and place a reverse proxy (nginx,
> Caddy) in front. See [Deployment](deployment.md).

### WAL Mode

Write-Ahead Logging allows concurrent reads and writes without blocking.
Enable it for production workloads:

```bash
bialet -w /www/myapp
```

### Resource Limits

Cap memory and CPU to contain runaway scripts:

```bash
# Soft limit 128 MB, hard limit 256 MB
# Soft limit 25% CPU, hard limit 50% CPU
bialet -m 128 -M 256 -c 25 -C 50 /www/myapp
```

When the soft limit is exceeded, the server logs a warning. When the hard
limit is exceeded, the process is killed.

### Logging

Route all output to a file:

```bash
bialet -l /var/log/bialet.log /www/myapp
```

When logging to a file, ANSI color codes are automatically disabled. For
live monitoring, leave `-l` out and pipe `stdout` or use `systemd`'s journal.

### Custom Ignored Files

Override the default ignored-file patterns:

```bash
bialet -i "*.md,*.txt,Dockerfile*" /www/myapp
```

Files matching these patterns are never served through HTTP. The `_` and
`.` prefix protection is separate and always active — ignored files is an
additional layer on top.

## No .env Files

Bialet does not read `.env` files. Configuration is database-backed by
design. This means:

- No parsing, no precedence rules, no "which `.env` file is active?"
- Configuration is environment-specific because each environment has its
  own database
- Config changes persist across server restarts — they're just rows in
  SQLite
- You can build config UIs by writing to `BIALET_CONFIG` from your app

If you need to inject secrets at startup (API keys, tokens), seed them into
the database from your deployment script:

```bash
# In your deploy script
bialet -r 'Config.set("stripe_key", "sk_live_...")' /www/myapp
```

The `-r` flag executes the given Wren code and exits before starting the
server.

## Common Patterns

### Feature Flags

```wren
var betaEnabled = Config.bool("beta_features") || false
if (betaEnabled) {
  // Show beta UI
}
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
return Template.new(title).layout(<main>
  <h1>Welcome</h1>
</main>)
```

## Pitfalls

- **`Config.get()` returns `null` for missing keys** — always provide a
  fallback: `Config.get("key") || "default"`.
- **Config values are strings** — use `Config.num()` and `Config.bool()`
  for typed access. `Config.get("port")` returns `"7001"`, not `7001`.
- **No environment variables** — Bialet doesn't read `process.env` or
  `.env` files. Everything is in the database.
- **Database-specific environments** — swapping `-d` changes your config
  table. Keep your production database file out of version control.
- **CLI flags override nothing in the database** — CLI flags set the server
  environment (port, host, limits). Config values in `BIALET_CONFIG` are
  app-level settings (titles, feature flags, pagination). They don't
  conflict because they address different concerns.

## Next Steps

- Learn about [Database](database.md) — the Query object, migrations, and
  configuration defaults
- Set up a production environment with [Deployment](deployment.md)
- See the [Security](security.md) guide for CSRF, headers, and secure
  configuration
- Check the [Reference](reference.md) for the full `Config` API
