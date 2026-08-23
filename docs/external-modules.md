# External Modules

Bialet supports importing external Wren modules from remote sources, so
you can use community-created libraries without manually downloading and
managing them. This page covers using them, authoring your own, and
managing the cache.

## Import Syntax

### 1. GitHub Shorthand (Recommended)

```wren
import "gh:owner/repo/path/to/file" for ClassName
```

Fetches from the `main` branch by default. Target a specific branch or tag
with `@`:

```wren
import "gh:owner/repo/path/to/file@branch-or-tag" for ClassName
```

```wren
// Import emoji utilities from main branch
import "gh:4lb0/emoji/emoji" for Emoji

// Import from specific version/branch (recommended for stability)
import "gh:4lb0/emoji/export@1.0" for Emoji as EmojiV1

// Import from a specific tag
import "gh:username/mylib/module@v2.1.0" for MyClass
```

- The `.wren` extension is **automatically appended** — don't include it
  in the import path.
- Default branch is `main` if you don't specify one.
- Use `@branch` or `@tag` to pin a specific version.

### 2. Full URL

```wren
import "https://example.com/path/to/module.wren" for ClassName
```

- You **must** include the `.wren` extension in the URL.
- The URL must return raw Wren source code, not HTML.
- For GitHub, use `raw.githubusercontent.com` URLs.

```wren
import "https://raw.githubusercontent.com/4lb0/emoji/main/emoji.wren" for Emoji
```

## How It Works

1. **Cache check** — Bialet checks whether the module is already cached in
   the `BIALET_REMOTE_MODULES` database table.
2. **Download** — if not cached, it downloads the module via HTTP GET.
3. **Validation** — checks for a 2xx status code.
4. **Store** — the content is stored in the database, keyed by import
   path.
5. **Load** — the module is loaded and made available to your code.

The first load needs an internet connection; every load after that reads
from the local database — fast, and works offline. Cached modules persist
until you explicitly clear them.

> ⚠️ Pitfall: cached modules don't auto-update. If you push a fix to the
> `main` branch of an imported module, running instances keep using the
> old cached copy until you clear the cache. This is deliberate — treat it
> as a stability feature, not a bug.

## When to Use an External Module

Bialet's philosophy here is the opposite of the Node ecosystem. Node
culture normalizes pulling in a package for a one-line function (`is-odd`,
`left-pad`) because `npm install` is nearly free and the dependency graph
is invisible day-to-day. In Bialet, every `import "gh:..."` is a real,
visible network fetch the first time it runs, a row cached forever in
*your* app's SQLite file, and code that runs with the full privileges of
your app.

A one-line helper doesn't clear that bar. If a function fits in a few
lines and has no dependencies of its own, write it directly in your domain
class or a local `_app/*.wren` file — it costs nothing to maintain, has no
network dependency, and can't change or disappear out from under you.

Reach for an external import when the library does something genuinely
substantial: nontrivial parsing, a sizable data set, an algorithm you
don't want to re-implement and verify yourself. Reserve it for real
dependencies, not for saving five lines of typing.

## Authoring an External Module

There is no package format, manifest, or build step. An external module
is just a plain `.wren` file, reachable over HTTP, that defines one or
more classes (or top-level vars) with a `construct`/static API — exactly
like any local `_app/*.wren` file you'd `import` in your own project.

### Minimal Example

A single-file library published on GitHub, e.g. `4lb0/emoji/emoji.wren`:

```wren
// emoji.wren — the entire module
class Emoji {
  static shrug { "¯\\_(ツ)_/¯" }
  static wave(name) { "👋 %(name)" }
}
```

Consumers import it with:

```wren
import "gh:4lb0/emoji/emoji" for Emoji
System.print(Emoji.wave("world"))
```

That's the whole contract: push a `.wren` file to a public repo (or serve
it from any URL that returns raw Wren source), and it's importable.

### Conventions for Publishing

- **One purpose per file.** Keep the module focused; consumers only pay
  the cache/download cost for files they actually import.
- **PascalCase class names**, matching the rest of the Wren style used
  throughout Bialet apps.
- **Avoid side effects at the top level.** Code outside a class body runs
  the moment the module loads. Stick to class and method definitions —
  don't run queries, print logs, or mutate state just by being imported.
- **Tag releases with git tags** (`v1.0`, `v1.1`, ...) instead of only
  publishing to `main`. Consumers pin `@v1.0` for stability; you keep
  `main` for in-progress work. This is for *your* consumers' benefit —
  see [Cache Management](#cache-management) for why pinning matters on
  the consuming side.
- **Document the exported API** with a `README.md` in the repo (a Wren
  doc-comment format doesn't exist) — show the exact `import` line, since
  that's the one thing every consumer needs verbatim.
- **Test it like any Bialet app** before publishing: point `bialet -t` at
  the file with a throwaway app root and exercise the exported classes.
  See [Tests](tests.md).

### Multi-File Modules and Self-Imports

This is the part that trips people up: **a relative import inside a
remote module does not resolve relative to the remote module.**

Bialet resolves relative imports (any import path without a colon) against
the directory of the top-level `.wren` file that's handling the current
request — fixed once per request, regardless of how many files or remote
modules get imported along the way. A module fetched from GitHub has no
real position in your app's directory tree, so if its own source does a
plain relative import, Bialet resolves that path against *your app's*
root, not the module's repo. Best case, the file doesn't exist there and
you get an import error; worst case, your app happens to have a
same-named local file and the module silently loads the wrong code.

```wren
// ❌ Wrong — "helper" resolves against the consuming app's directory,
// not against this module's own repo.
// File: gh:someuser/mylib/main.wren
import "helper" for Helper
```

```wren
// ✅ Correct — reference your own repo explicitly, the same way any
// consumer would.
// File: gh:someuser/mylib/main.wren
import "gh:someuser/mylib/helper" for Helper
```

If you pin versions internally, keep the tag consistent with the one
you're publishing, so a given tag of `main.wren` always pulls the matching
tag of `helper.wren`:

```wren
// File: gh:someuser/mylib/main.wren, tagged v1.0
import "gh:someuser/mylib/helper@v1.0" for Helper
```

> ⚠️ Pitfall: if you tag a new release and forget to bump the internal
> `@tag` references inside the module itself, older files can end up
> importing a newer (or newer files an older) internal helper than
> intended. Bump every internal self-reference together with the release
> tag.

## Cache Management

External modules are cached in `_db.sqlite3`, table `BIALET_REMOTE_MODULES`:

```sql
CREATE TABLE IF NOT EXISTS BIALET_REMOTE_MODULES (
  module TEXT PRIMARY KEY,      -- The import path (e.g., "gh:user/repo/path")
  content TEXT,                 -- The cached Wren source code
  createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
)
```

```sql
-- Clear all cached external modules (forces re-download on next import)
DELETE FROM BIALET_REMOTE_MODULES;

-- View all cached modules
SELECT module, createdAt FROM BIALET_REMOTE_MODULES;

-- Check cache size
SELECT COUNT(*), SUM(LENGTH(content)) as total_bytes
FROM BIALET_REMOTE_MODULES;
```

After clearing the cache, restart your Bialet application (or trigger a
reload) to re-download the modules.

### Refreshing a Module Programmatically

Since the cache is just a table, you can clear a specific row from Wren
code with a normal `Query` object — no shell access needed:

```wren
`DELETE FROM BIALET_REMOTE_MODULES WHERE module = ?`.query("gh:yourorg/internal-lib/utils@main")
```

The next time any request imports that module, the cache miss triggers a
fresh HTTP download. This is most useful for **internal libraries you
control**, shared across several Bialet apps, where you want changes to
propagate without touching every consumer's database by hand. Trigger it
from a periodic cron job:

```wren
// _cron.wren
// Pull the latest internal shared library once a day at 3 AM.
Cron.at(3, 0) { |d|
  `DELETE FROM BIALET_REMOTE_MODULES WHERE module = ?`
    .query("gh:yourorg/internal-lib/utils@main")
}
```

Or from an admin-only route, so a person can force a refresh on demand:

```wren
// _app/admin/refresh-module.wren
if (Request.isPost) {
  var mod = Request.post("module") || ""
  if (mod != "") {
    `DELETE FROM BIALET_REMOTE_MODULES WHERE module = ?`.query(mod)
  }
  return Response.redirect("/admin/refresh-module?done=1")
}
```

> ⚠️ Pitfall: this deliberately undoes the stability/security guarantee
> described above — a module can now change between requests without a
> deploy. Only wire this up for modules you own or fully trust, and never
> for third-party libraries pinned to `main` for convenience.

## GitHub URL Format

`gh:owner/repo/path@branch` is internally converted to:

```text
https://raw.githubusercontent.com/owner/repo/refs/heads/branch/path.wren
```

| Import Statement | Generated URL |
| --- | --- |
| `gh:user/lib/utils@dev` | `https://raw.githubusercontent.com/user/lib/refs/heads/dev/utils.wren` |
| `gh:org/pkg/sub/module@v1.0` | `https://raw.githubusercontent.com/org/pkg/refs/heads/v1.0/sub/module.wren` |

- The `.wren` extension is added automatically.
- Default branch is `main` if unspecified.
- The path must resolve to a valid Wren file in the repository.
- Invalid paths or missing files trigger error messages in the logs.

## Error Handling

**"Invalid GitHub URL"** — the import path doesn't follow
`gh:owner/repo/path`; owner, repo, or file path is missing.

**"Module not found in GitHub"** — file doesn't exist at that path, the
branch/tag doesn't exist, the HTTP request returned a non-2xx status, or
there's a network problem.

**"Import type not supported"** — the import uses a protocol other than
`gh:`, `http://`, or `https://`.

Check the Bialet logs for details:

```wren
System.print("Debug: attempting import...")
```

## Security Considerations

> ⚠️ Pitfall: external imports download and **execute** code from remote
> sources, with the same privileges as your application. Treat an import
> statement like adding a dependency, not like a link.

- **Verify the source** before importing.
- **Review the code** on GitHub when possible.
- **Use version tags**, not `main`, for stability and predictability.
- **Cache behavior is a security feature** — once downloaded, a module
  won't silently change under you.

```wren
// ✅ Good: well-known, maintained library, pinned version
import "gh:4lb0/emoji/emoji@1.0" for Emoji

// ❌ Avoid: unknown source, unpinned, unverified
import "gh:random-user/suspicious-lib/module" for SomeClass
```

## Multiple Versions

```wren
import "gh:user/lib/module" for Module as ModuleLatest
import "gh:user/lib/module@v1.0" for Module as ModuleV1

// Use specific version based on your needs
var result = ModuleV1.someFunction()
```

## Troubleshooting

**Module not found:**
- Visit the GitHub URL in a browser to confirm the path exists.
- Check your internet connection (required for first-time downloads).
- Confirm the branch name — defaults to `main`, not `master`.

**Import fails silently:**
- Check the Bialet server logs.
- Confirm the module returns raw Wren code, not HTML or an error page.
- Try the full URL format to isolate GitHub-shorthand issues.

**Cached version is outdated:**

```sql
DELETE FROM BIALET_REMOTE_MODULES WHERE module LIKE 'gh:user/repo%';
```

Then restart your Bialet application. Prefer version tags over branches
going forward.

**"Import type not supported":**
- Use `gh:`, `http://`, or `https://` only — no `ftp://`, no `file://`.
- Check for typos in the import statement.

## Performance Tips

1. Use version tags to avoid unnecessary cache invalidation.
2. Import only what you need — every import adds to first-load time.
3. Pre-cache modules in development before deploying.
4. Monitor cache size if you import many large modules.

## Key Takeaways

- **`gh:` shorthand or full URLs** — both cached in `BIALET_REMOTE_MODULES`,
  never auto-updating once cached.
- **Write small helpers locally** — a one-line function doesn't justify a
  remote dependency; reserve external imports for substantial libraries.
- **Relative imports inside a remote module don't work** — a module's own
  internal imports must use full `gh:`/URL syntax pointing at itself.
- **You can force a refresh** by deleting the module's row from
  `BIALET_REMOTE_MODULES` via a normal `Query`, but that trades away the
  stability guarantee — only do it for libraries you own.
