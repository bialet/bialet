# Project Structure and Routing

## Overview

Bialet uses a **file-based routing system** inspired by static HTML websites.
Each `.wren` file in your project corresponds to a URL path, making it intuitive
to organize and understand your application structure.

**Philosophy:** Think of your Bialet project as a static HTML website first,
then add dynamic behavior with Wren code where needed.

## Basic Routing Rules

### 1. Direct File Mapping

Each `.wren` file maps directly to a URL path, similar to how static HTML files
work:

- **File:** `contact-us.wren`
- **URLs:**
  - `http://127.0.0.1:7001/contact-us.wren`
  - `http://127.0.0.1:7001/contact-us` (extension optional)

- **File:** `landing/newsletter/cool-campaign.wren`
- **URL:** `http://127.0.0.1:7001/landing/newsletter/cool-campaign`

### 2. Special Files (Protected from Direct Access)

Files and folders starting with `_` or `.` are **protected** and cannot be
accessed directly via URL.

#### Preferred Structure: `_app/` Folder

The recommended approach is to organize special files inside an `_app/`
directory:

- `_app/template.wren` — Application-wide template (header, footer, nav)
- `_app/migration.wren` — Database migrations
- `_app/cron.wren` — Scheduled tasks
- `_app/domain.wren` — Domain-specific configuration (optional)
- `_db.sqlite3` — SQLite database file

> **Note on Configuration:** Bialet doesn't use `.env` files. Configuration is
> stored in the `BIALET_CONFIG` table within your SQLite database
> (`_db.sqlite3`). Since each environment uses its own SQLite database file,
> your configuration is naturally environment-specific. See the
> [Config class reference](reference.md) for managing configuration values.

#### Alternative: Root-Level Special Files

You can also place special files directly at the root (prefixed with `_`):

- `_app.wren` — Application-wide template
- `_migration.wren` — Database migrations
- `_cron.wren` — Scheduled tasks
- `_route.wren` — Dynamic route handler (explained below)

**Both approaches work**, but the `_app/` folder keeps your root directory
cleaner and more organized.

**Security Note:** Any file or folder starting with `_` or `.` (like `_app/`,
`_private/`) returns **403 Forbidden** when accessed directly via URL.

**Ignored Files:** Certain files are automatically ignored and won't be served:
`README*`, `AGENTS*`, `LICENSE*`, `*.json`, `*.yml`, and `*.yaml`. This allows
you to keep documentation, AI agent instructions, and configuration files in
your project without exposing them.

### 3. Dynamic Routing with `_route.wren`

For dynamic URL segments (like user IDs, slugs, or API paths), create a
`_route.wren` file in the relevant directory.

**Example:** Create `api/_route.wren` to handle dynamic API routes:

```wren
// File: api/_route.wren
// Handles URLs like: /api/users/123 or /api/posts/my-slug

var segment = Request.route(0)  // First dynamic segment
var id = Request.route(1)       // Second dynamic segment

if (segment == "users") {
  var userId = Num.fromString(id)
  var user = `SELECT * FROM users WHERE id = ?`.first([userId])
  Response.out(user)

} else if (segment == "posts") {
  var slug = id
  var post = `SELECT * FROM posts WHERE slug = ?`.first([slug])
  Response.out(post)
}
```

**URL Examples:**

| URL                              | `Request.route(0)` | `Request.route(1)` | `Request.get("fields")` |
| -------------------------------- | ------------------ | ------------------ | ------------------------- |
| `/api/users/1`                   | `"users"`          | `"1"`              | `null`                    |
| `/api/posts/hello-world`         | `"posts"`          | `"hello-world"`    | `null`                    |
| `/api/users/1?fields=name,email` | `"users"`          | `"1"`              | `"name,email"`            |

> **Note:** You don't need a single `_route.wren` file for all routing. Create
> separate `_route.wren` files in different directories as needed.

## Complete Example Project Structure

Here's a practical example of a blog application structure using the **preferred
`_app/` folder approach**:

```text
my-blog/
├── _app/                  # Protected folder for app configuration
│   ├── template.wren      # Site-wide template (header, footer, nav)
│   ├── migration.wren     # Database schema setup
│   ├── cron.wren          # Scheduled tasks (optional)
│   └── domain.wren        # Domain config (optional)
│
├── _db.sqlite3            # SQLite database
│
├── index.wren             # Homepage (/)
├── about.wren             # About page (/about)
├── contact.wren           # Contact page (/contact)
│
├── blog/
│   ├── index.wren         # Blog list (/blog)
│   └── _route.wren        # Dynamic post handler (/blog/post-slug)
│
├── admin/
│   ├── index.wren         # Admin dashboard (/admin)
│   ├── login.wren         # Admin login (/admin/login)
│   └── posts/
│       ├── index.wren     # Post list (/admin/posts)
│       ├── new.wren       # Create post (/admin/posts/new)
│       └── _route.wren    # Edit post (/admin/posts/:id)
│
├── api/
│   └── _route.wren        # API endpoints (/api/*)
│
│
├── css/
│   └── style.css          # Static CSS
│
└── js/
    └── main.js            # Static JavaScript
```

**URL Mapping for this structure:**

| URL                   | File Executed             | Purpose                                  |
| --------------------- | ------------------------- | ---------------------------------------- |
| `/`                   | `index.wren`              | Homepage                                 |
| `/about`              | `about.wren`              | About page                               |
| `/blog`               | `blog/index.wren`         | Blog post list                           |
| `/blog/my-first-post` | `blog/_route.wren`        | Single blog post (slug: "my-first-post") |
| `/admin`              | `admin/index.wren`        | Admin dashboard                          |
| `/admin/posts/new`    | `admin/posts/new.wren`    | Create new post form                     |
| `/admin/posts/123`    | `admin/posts/_route.wren` | Edit post with ID 123                    |
| `/api/posts/456`      | `api/_route.wren`         | API endpoint for post 456                |
| `/_app`               | ❌ **403 Forbidden**      | Protected file                           |

### Example: Blog Post Handler

**File:** `blog/_route.wren`

```wren
// Import shared layout from _app folder
import "_app/template" for App

var slug = Request.route(0)

if (!slug) {
  Response.redirect("/blog")
  return
}

// Fetch post from database
var post = `
  SELECT title, content, createdAt, author
  FROM posts
  WHERE slug = ? AND published = 1
`.query(slug).fetch()

if (!post) {
  Response.status(404)
  Response.out("<h1>Post not found</h1>")
  return
}

// Render using App template
Response.out(App.render {
  <article>
    <h1>{{post["title"]}}</h1>
    <p class="meta">By {{post["author"]}} on {{post["createdAt"]}}</p>
    <div class="content">
      {{post["content"]}}
    </div>
  </article>
})
```

> **Note:** If using the root-level structure, import from `_app` instead:
> `import "_app" for App`

## Key Takeaways

✅ **File-based routing** — Each `.wren` file is a route (just like HTML)  
✅ **Protected files** — Files starting with `_` or `.` are hidden from direct
access  
✅ **Dynamic routes** — Use `_route.wren` + `Request.route(N)` for dynamic
segments  
✅ **Flexible structure** — Organize files however makes sense for your
project  
✅ **No configuration needed** — Routing works automatically based on file
structure

## External Imports

Bialet supports importing external Wren modules from remote sources. This allows
you to use community-created libraries and modules without having to manually
download and manage them.

### Import Syntax

There are two ways to import external modules:

#### 1. GitHub Shorthand (Recommended)

The most common way is using the `gh:` prefix for GitHub repositories:

```wren
import "gh:owner/repo/path/to/file" for ClassName
```

This will fetch the module from the `main` branch by default. You can also
specify a different branch or tag:

```wren
import "gh:owner/repo/path/to/file@branch-or-tag" for ClassName
```

**Examples:**

```wren
// Import from main branch (recommended for latest version)
import "gh:bialet/extra/mcp" for Mcp

// Import emoji utilities from main branch
import "gh:4lb0/emoji/emoji" for Emoji

// Import from specific version/branch (recommended for stability)
import "gh:4lb0/emoji/export@1.0" for Emoji as EmojiV1

// Import from a specific tag
import "gh:username/mylib/module@v2.1.0" for MyClass
```

**Important Notes:**

- The `.wren` extension is **automatically appended** - do not include it in the
  import path
- Default branch is `main` if not specified with `@branch`
- Use `@branch` or `@tag` to target specific versions

#### 2. Full URL

You can also import from any URL that returns raw Wren code:

```wren
import "https://example.com/path/to/module.wren" for ClassName
```

When using full URLs:

- You **must** include the `.wren` extension in the URL
- The URL must return raw Wren source code (not HTML)
- For GitHub, use `raw.githubusercontent.com` URLs

**Example:**

```wren
import "https://raw.githubusercontent.com/4lb0/emoji/main/emoji.wren" for Emoji
```

### How It Works

When Bialet encounters an external import:

1. **Cache Check**: First, it checks if the module is already cached in the
   `BIALET_REMOTE_MODULES` database table
2. **Download**: If not cached, it downloads the module from the specified URL
   using an HTTP GET request
3. **Validation**: Checks for HTTP 2xx status codes (200-299) to ensure
   successful download
4. **Store**: The content is stored in the database with the import path as the
   key
5. **Load**: The module is loaded and made available to your code

This means:

- **First load**: Requires an internet connection to download the module
- **Subsequent loads**: Use the cached version from the database (no internet
  needed)
- **Performance**: After the first download, imports are fast since they're
  served from the local database
- **Persistence**: Cached modules remain in the database until explicitly
  cleared

### Cache Management

External modules are cached in your application's SQLite database
(`_db.sqlite3`) in the `BIALET_REMOTE_MODULES` table.

**Table Schema:**

```sql
CREATE TABLE IF NOT EXISTS BIALET_REMOTE_MODULES (
  module TEXT PRIMARY KEY,      -- The import path (e.g., "gh:user/repo/path")
  content TEXT,                 -- The cached Wren source code
  createdAt DATETIME DEFAULT CURRENT_TIMESTAMP
)
```

**To refresh cached modules:**

```sql
-- Clear all cached external modules (forces re-download on next import)
DELETE FROM BIALET_REMOTE_MODULES;

-- Clear a specific module
DELETE FROM BIALET_REMOTE_MODULES WHERE module = 'gh:bialet/extra/mcp';

-- View all cached modules
SELECT module, createdAt FROM BIALET_REMOTE_MODULES;

-- Check cache size
SELECT COUNT(*), SUM(LENGTH(content)) as total_bytes
FROM BIALET_REMOTE_MODULES;
```

After clearing the cache, restart your Bialet application or trigger a reload to
re-download the modules.

### GitHub URL Format

The GitHub shorthand `gh:owner/repo/path@branch` is internally converted to:

```text
https://raw.githubusercontent.com/owner/repo/refs/heads/branch/path.wren
```

**Conversion Examples:**

| Import Statement             | Generated URL                                                               |
| ---------------------------- | --------------------------------------------------------------------------- |
| `gh:bialet/extra/mcp`        | `https://raw.githubusercontent.com/bialet/extra/refs/heads/main/mcp.wren`   |
| `gh:user/lib/utils@dev`      | `https://raw.githubusercontent.com/user/lib/refs/heads/dev/utils.wren`      |
| `gh:org/pkg/sub/module@v1.0` | `https://raw.githubusercontent.com/org/pkg/refs/heads/v1.0/sub/module.wren` |

Notes:

- The `.wren` extension is automatically added by Bialet
- Default branch is `main` if not specified
- The path must lead to a valid Wren file in the repository
- Invalid paths or missing files will trigger error messages in the logs

### Error Handling

Bialet provides clear error messages for import failures:

**"Invalid GitHub URL"**:

- The import path doesn't follow the `gh:owner/repo/path` format
- Missing owner, repository, or file path components

**"Module not found in GitHub"**:

- The file doesn't exist at the specified path
- The branch/tag doesn't exist
- HTTP request returned non-2xx status code
- Network connectivity issues

**"Import type not supported"**:

- The import uses a protocol other than `gh:`, `http://`, or `https://`

Check the Bialet logs for detailed error messages:

```wren
System.print("Debug: attempting import...")
```

### Security Considerations

**Important**: External imports download and execute code from remote sources.
Only import modules from trusted sources.

- **Verify the source**: Check the repository and author before importing
- **Review the code**: When possible, review the module's source code on GitHub
- **Use version tags**: Prefer importing from specific branches or tags rather
  than `main` to ensure stability and prevent unexpected changes
- **Cache behavior**: Once downloaded, modules are cached and won't update
  automatically - this is a security feature
- **Trust implications**: Imported code runs with the same privileges as your
  application

**Best Practices:**

```wren
// ✅ Good: Using a specific version tag
import "gh:bialet/extra/mcp@v1.0.0" for Mcp

// ⚠️ Caution: Using main branch (may change over time)
import "gh:bialet/extra/mcp" for Mcp

// ✅ Good: Well-known, maintained library
import "gh:4lb0/emoji/emoji@1.0" for Emoji

// ❌ Avoid: Unknown sources without verification
import "gh:random-user/suspicious-lib/module" for SomeClass
```

### Example Usage

Here's a complete example using external imports:

```wren
// Import external MCP module from a specific version
import "gh:bialet/extra/mcp@v1.0" for Mcp

// Import emoji library from main branch
import "gh:4lb0/emoji/emoji" for Emoji

#!doc = "A greeting tool with emojis"
class Greet {
  construct new(params) {
    _name = params["name"]
  }

  #!doc = "Name of the person to greet"
  #!required
  name(name) { _name = name }

  call() {
    return "%(Emoji.wave) Hello, %(_name)! %(Emoji.heart)"
  }
}

var mcp = Mcp.new('emoji-greeter', '1.0.0')
mcp.addTool(Greet)
mcp.serve
```

### Multiple Versions

You can import different versions of the same module using aliases:

```wren
import "gh:user/lib/module" for Module as ModuleLatest
import "gh:user/lib/module@v1.0" for Module as ModuleV1

// Use specific version based on your needs
var result = ModuleV1.someFunction()
```

### Troubleshooting

**Module not found errors:**

- Verify the GitHub repository and file path exist by visiting the URL in a
  browser
- Check your internet connection (required for first-time downloads)
- Ensure the branch name is correct (defaults to `main`, not `master`)
- Verify the file exists at the exact path specified

**Import fails silently:**

- Check the Bialet server logs for error messages
- Verify the module returns valid Wren code (not HTML or an error page)
- Ensure the URL is accessible and returns raw content
- Try the full URL format to see if it's a GitHub-specific issue

**Cached version is outdated:**

- Clear the module from the database cache:

  ```sql
  DELETE FROM BIALET_REMOTE_MODULES WHERE module LIKE 'gh:user/repo%';
  ```

- Restart your Bialet application to fetch the latest version
- Consider using version tags instead of branches for stability

**"Import type not supported" error:**

- Ensure you're using `gh:`, `http://`, or `https://` protocols
- Other protocols (ftp, file, etc.) are not supported
- Check for typos in the import statement

### Performance Tips

1. **Use version tags** to avoid cache invalidation and ensure consistent
   behavior
2. **Import only what you need** - each import adds to startup time on first
   load
3. **Pre-cache modules** in development by importing them once before deployment
4. **Monitor cache size** if importing many large modules
