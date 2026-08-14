# Live Reload

When enabled, Bialet injects a small polling script into every HTML response.
The script checks for file changes once per second and reloads the page
automatically — no browser extensions, no WebSocket server, no extra process.

## Enabling

Live reload is controlled by the `BIALET_LIVE_RELOAD` configuration key.

The easiest way to turn it on is `bialet dev`, which also enables the
in-browser error display, serves the current directory, and opens your browser
(see [Getting Started](../getting-started/2-setup)). It is stored in the
database, so it stays on for later `bialet` runs until you disable it.

Or set it once in your development database:

```bash
bialet -r 'Config.enable("BIALET_LIVE_RELOAD")' .
```

Or directly:

```bash
bialet -r 'Config.set("BIALET_LIVE_RELOAD", "1")' .
```

Disable it:

```bash
bialet -r 'Config.disable("BIALET_LIVE_RELOAD")' .
```

Since configuration lives in the SQLite database, enabling it in dev has no
effect on production — each environment has its own `_db.sqlite3` and its own
`BIALET_CONFIG` table.

> ⚠️ Pitfall: `BIALET_LIVE_RELOAD` is read when the server starts. Enabling or
> disabling it while a server is running requires a restart.

To check if it's currently active:

```wren
if (Config.bool("BIALET_LIVE_RELOAD")) {
  System.print("Live reload is on")
}
```

## How It Works

### Script injection

When live reload is enabled, Bialet appends a `<script>` tag to every HTML
response, right before `</body>` (or at the end if no `</body>` is present).
The script polls the `/_livereload` endpoint every second.

### The `/_livereload` endpoint

This is an internal route handled by the server before any Wren code runs.
It returns a plain-text version number that updates whenever a file in the
app directory changes.

```
GET /_livereload → 200 OK
Content-Type: text/plain

1746554321
```

When the browser sees a different version number from its last poll, it calls
`location.reload()`.

### File watching

Bialet uses inotify (on Linux) to watch the app directory for changes. When
any file is created, modified, or deleted, the version number updates and the
next poll picks it up.

The Wren VM also reloads automatically on `.wren` file changes — this is
independent of live reload and happens regardless of the config setting.

## Limitations

- **Polling, not push.** There's a 1-second delay between saving a file and
  the browser reloading. For CSS-only changes, consider a tool that injects
  stylesheets directly.
- **No WebSocket.** Bialet speaks HTTP/1.0 and runs in a single process.
  Polling is the simplest approach that works everywhere without adding
  dependencies.
- **Full page reload only.** The script calls `location.reload()`. State
  in JavaScript variables or form inputs is lost.

> ⚠️ Pitfall: Enabling live reload on a production database injects the
> script into every page served to real users. The `/_livereload` endpoint
> becomes publicly accessible. Only enable it in development.
