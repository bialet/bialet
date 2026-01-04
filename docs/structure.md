# Project Structure and Routing

**The best way to think a project with Bialet is do it first like static old
school HTML, then replace the logic and template duplication with Wren code.**

The scripts will be load as if they were HTML files, so the file
`contact-us.wren` can be open with the URL
[127.0.0.1:7001/contact-us.wren](http://127.0.0.1:7001/contact-us.wren) or even
without the wren suffix
[127.0.0.1:7001/contact-us](http://127.0.0.1:7001/contact-us).

This will work with each folder, for example the URL
[127.0.0.1:7001/landing/newsletter/cool-campaign](http://127.0.0.1:7001/landing/newsletter/cool-campaign)
will run the script `landing/newsletter/cool-campaign.wren`. Like if there was
an HMTL file.

That mean that each file is public and will be executed **except** when it start
with a `_` or `.`. The `_app.wren` file won't be load with the URL
[127.0.0.1:7001/\_app](http://127.0.0.1:7001/_app) or with any other URL. Any
file or any file under a folder that start with `_` or `.` won't be open.

For dynamic routing, add a `_route.wren` file in the folder and use the
`Request.route(N)` function to get the value. **N** is the position of the route
parameter. For the file `api/_route.wren` when called from
[127.0.0.1:7001/api/users/1?fields=name,email&sortType=ASC](http://127.0.0.1:7001/api/users/1?fields=name,email&sortType=ASC)
those will be the values:

```wren

Request.route(0) // users
Request.route(1) // 1
Request.param("fields") // name,email
Request.param("sortType") // ASC
```

> **Note:** There is no need to add a single `_route.wren` file to handle all
> the routing.

## External Imports

Bialet supports importing external Wren modules from remote sources. This allows you to use community-created libraries and modules without having to manually download and manage them.

### Import Syntax

There are two ways to import external modules:

#### 1. GitHub Shorthand

The most common way is using the `gh:` prefix for GitHub repositories:

```wren
import "gh:owner/repo/path/to/file" for ClassName
```

This will fetch the module from the `main` branch by default. You can also specify a different branch:

```wren
import "gh:owner/repo/path/to/file@branch" for ClassName
```

**Examples:**

```wren
// Import from main branch
import "gh:bialet/extra/mcp" for Mcp

// Import emoji utilities
import "gh:4lb0/emoji/emoji" for Emoji

// Import from specific branch (version 1.0)
import "gh:4lb0/emoji/export@1.0" for Emoji as EmojiV1
```

#### 2. Full URL

You can also import from any URL that returns raw Wren code:

```wren
import "https://example.com/path/to/module.wren" for ClassName
```

**Example:**

```wren
import "https://raw.githubusercontent.com/4lb0/emoji/main/emoji.wren" for Emoji
```

### How It Works

When Bialet encounters an external import:

1. **Cache Check**: First, it checks if the module is already cached in the database
2. **Download**: If not cached, it downloads the module from the specified URL
3. **Store**: The content is stored in the database for future use
4. **Load**: The module is loaded and made available to your code

This means:

- **First load**: Requires an internet connection to download the module
- **Subsequent loads**: Use the cached version from the database (no internet needed)
- **Performance**: After the first download, imports are fast since they're served from the local database

### Cache Management

External modules are cached in your application's SQLite database (`_db.sqlite3`). To refresh cached modules:

```sql
-- Clear all cached external modules
DELETE FROM BIALET_EXTERNAL_MODULES;

-- Clear a specific module
DELETE FROM BIALET_EXTERNAL_MODULES WHERE url = 'gh:bialet/extra/mcp';
```

### GitHub URL Format

The GitHub shorthand `gh:owner/repo/path@branch` is internally converted to:

```text
https://raw.githubusercontent.com/owner/repo/refs/heads/branch/path.wren
```

Notes:

- The `.wren` extension is automatically added if not present
- Default branch is `main` if not specified
- The module must be a valid Wren file

### Security Considerations

**Important**: External imports download and execute code from remote sources. Only import modules from trusted sources.

- **Verify the source**: Check the repository and author before importing
- **Review the code**: When possible, review the module's source code
- **Use version tags**: Prefer importing from specific branches or tags rather than `main` to ensure stability
- **Cache behavior**: Once downloaded, modules are cached and won't update automatically

### Example Usage

Here's a complete example using external imports:

```wren
// Import external MCP module
import "gh:bialet/extra/mcp" for Mcp

// Import emoji library
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

### Troubleshooting

**Module not found errors:**

- Verify the GitHub repository and file path exist
- Check your internet connection (for first-time downloads)
- Ensure the branch name is correct (defaults to `main`)

**Import fails silently:**

- Check the Bialet logs for error messages
- Verify the module returns valid Wren code
- Ensure the URL is accessible and returns raw content

**Cached version is outdated:**

- Clear the module from the database cache
- Restart your Bialet application to fetch the latest version
