# Database

Bialet includes the [SQLite](https://www.sqlite.org/) library for its database.

## Configuration

There is no configuration other than naming the database file.

The default database name is `_db.sqlite3` placed in the root directory.

To change the name of the database, set the `-d` option.

For production, enable Write-Ahead Logging with the `-w` flag to allow
concurrent reads and writes without blocking. See the
[Deployment Guide](sqlite-wal-mode-production) for details.

**There is no integration with other databases at the moment.**

## Query Object

Use backticks to surround SQL statements `` ` ` ``.

The Query object provides several methods to retrieve results:

- `query()`: Returns the last inserted id if the query was an `INSERT` statement.
- `fetch()`: Returns the result of the query as an array (List object).
- `first()`: Returns the first result of the query as an object (Map object).
- `val()`: Returns the value of the first column as a string.
- `toNum()`: Returns the value of the first column as a number.
- `toBool()`: Returns the value of the first column as a boolean.

Additional methods available on Query objects:
- `save(values)`: Insert or update a row in the table. See [Insert and update](#insert-and-update).
- `order(column, direction, allowedColumns, limit)`: Append a safe ORDER BY clause. See [Safe Sorting](#safe-sorting-with-order).
- `to(Class)`: Map results to domain class instances. See [Mapping Results](#mapping-results-to-domain-classes).

The `first()`, `val`, `toNum`, and `toBool` methods automatically add a LIMIT clause.

You can't concatenate strings or use interpolations with the Query object. When
you need to add parameters, use placeholders `?` and send the parameters to the
method.

All methods receive the following parameters:

- `()`: No parameters.
- `(params)`: An array of parameters.
- `(param1)`: Any non array parameter is converted to String.
- `(param1, param2)`: Convert all parameters to String.
- `(param1, param2, param3)`: Convert all parameters to String.

They can also be used as a property: `.query`, `.fetch`, `.first`, `.val` and
`.toNum`.

If you need more parameters, use the array syntax
(`([param1, param2, param3, param4, param5, ...])`).

```wren
// This will fail
var id = 1
`SELECT * FROM users WHERE id = %(id)`.first(id)

// This is the correct way
var id = 1
`SELECT * FROM users WHERE id = ?`.first(id)

// Also works
var id = 1
var params = [id]
`SELECT * FROM users WHERE id = ?`.first(params)

// Give me the name only
var name = `SELECT name FROM users WHERE id = ?`.val(id)

// Give the current day of the year
var day = `SELECT strftime('%j', 'now')`.toNum
```

(safe-sorting-with-order)=

## Safe Sorting with .order()

The `.order()` method provides a safe way to add ORDER BY clauses to your queries by validating column names against an allowed list and normalizing the sort direction.

```wren
order(column, direction, allowedColumns)
order(column, direction, allowedColumns, limit)
```

Parameters:
- `column`: The column name to sort by
- `direction`: Sort direction ("asc" or "desc", case-insensitive)
- `allowedColumns`: Optional list of allowed column names for validation (can be null)
- `limit`: Optional number of results to limit (only added if > 0)

The method returns a new Query object with the ORDER BY clause appended. If the column is not in the allowed list, it defaults to the first allowed column. The direction is automatically validated and normalized to uppercase.

```wren
// Basic usage
var users = `SELECT * FROM users`.order("name", "asc", null).fetch

// With validation - only allow specific columns
var allowedSorts = ["id", "name", "email", "created_at"]
var sortCol = Request.get("sort") ? Request.get("sort") : "id"
var sortDir = Request.get("order") ? Request.get("order") : "asc"

var users = `SELECT * FROM users`
  .order(sortCol, sortDir, allowedSorts)
  .fetch

// Combined with filtering
var search = Request.get("search") ? Request.get("search") : ""
var users = `
  SELECT * FROM users 
  WHERE (? = '' OR name LIKE '%' || ? || '%')
`.order(sortCol, sortDir, allowedSorts).fetch([search, search])

// With limit for pagination
var topUsers = `SELECT * FROM users`
  .order("score", "desc", ["score", "created_at"], 10)
  .fetch

// Invalid columns are rejected
var users = `SELECT * FROM users`
  .order("malicious_col", "desc", ["id", "name"])
  .fetch
// Will use "id" instead of "malicious_col"
```

This method is especially useful for REST APIs where sort parameters come from user input, preventing SQL injection through column name manipulation. For paginating sorted results, combine with `LIMIT`/`OFFSET` as shown in the [Pagination](#pagination) section.

(pagination)=

## Pagination

Use `LIMIT` and `OFFSET` with parameterized queries to paginate results. Always pass `LIMIT` and `OFFSET` as `?` placeholders — never concatenate them into the query string — to prevent SQL injection through user-controlled numeric values.

Count total rows separately when you need metadata like total pages:

```wren
// Read page/limit from request (defaults: page 1, 20 per page)
var page = Num.fromString(Request.get("page") ? Request.get("page") : "1")
var limit = Num.fromString(Request.get("limit") ? Request.get("limit") : "20")
var offset = (page - 1) * limit

// Count total rows
var total = `SELECT COUNT(*) FROM users`.toNum

// Fetch one page
var users = `SELECT * FROM users ORDER BY id LIMIT ? OFFSET ?`.fetch([limit, offset])

// Common REST API pattern: return data + pagination metadata
Response.json({
  "data": users,
  "pagination": {
    "page": page,
    "limit": limit,
    "total": total,
    "pages": ((total + limit - 1) / limit).floor
  }
})
```

> **Note:** column values from queries come back as **strings**. Use `.toNum` to get a numeric value directly (as with `COUNT` above), or `Num.fromString()` to convert individual columns. See the Data Types section below for details.

For a quick "top N" query without counting, use `.order()` with its optional limit parameter instead of manual `LIMIT`/`OFFSET`:

```wren
var topUsers = `SELECT * FROM users`
  .order("score", "desc", ["score", "created_at"], 10)
  .fetch
```

See [Safe Sorting](#safe-sorting-with-order) for the full `.order()` API, which validates sort columns against an allowed list — essential when sort parameters come from user input.

(mapping-results-to-domain-classes)=

## Mapping Results to Domain Classes

Query results can be automatically mapped to domain classes using the `.to(Class)` method. This is useful for converting database rows into instances of your domain models.

```wren
// Define a domain class with a constructor that accepts a Map
class Post {
  construct new(data) {
    _id = data["id"]
    _title = data["title"]
    _content = data["content"]
  }
  
  id { _id }
  title { _title }
  content { _content }
}

// Map a single result to a domain class
var post = `SELECT * FROM posts WHERE id = ?`.first(1).to(Post)

// Map multiple results to domain classes
var posts = `SELECT * FROM posts`.fetch.to(Post)
// Returns a List where each element is a Post instance
```

The `.to(Class)` method works with:

- Query results from `.fetch()` (returns a List of class instances)
- Single results from `.first()` (returns a single class instance)
- Any Map or List of Maps

(insert-and-update)=

## Insert and update

You can use a regular SQL `INSERT` or `UPDATE` statement.

```wren
var userParams = ["John Doe", "john@example.com"]
var id = `INSERT INTO users (name, email) VALUES (?, ?)`.query(userParams)
```

But you can also use the `save` method on a table Query, sending a Map
object with the values.

```wren
var user = {"name": "John Doe", "email": "john@example.com"}
var id = `users`.save(user)
```

The same method also works for updating the row.

```wren
var user = {"name": "John Doe", "email": "john@example.com"}
user["id"] = 1
// This will update the row
`users`.save(user)
```

## Data Types and BLOB Support

SQLite supports several data types including TEXT, INTEGER, REAL, BLOB, and
NULL.

**Important**: When accessing column values from query results (`.fetch()`,
`.first()`), all values are returned as **strings**. Use the following methods
for numeric access:

- `.toNum()` — returns the first column value as a number. Most common for
  `COUNT(*)`, `SUM()`, and other aggregate queries.
- `.val()` + `Num.fromString()` — manually convert a string column to a number
- `.toBool()` — returns the first column value as a boolean

```wren
var count = `SELECT COUNT(*) as c FROM votes`.toNum    // returns a number
var age = Num.fromString(row["age"])                    // manual conversion
var active = `SELECT active FROM users WHERE id = ?`.toBool(1)  // returns boolean
```

The [Pagination](#pagination) section shows this in practice: `.toNum` for
`COUNT` totals, `Num.fromString` for converting `page`/`limit` parameters
from the query string.

**Note on BLOB data**: While BLOB (Binary Large Object) data is retrieved
correctly from SQLite, it may not be properly handled when passed to Wren code
due to Wren's string representation. If you need to work with binary data,
consider encoding it as base64 or using another text-based representation.

## Migrations

The migration file can be in the root and be called `_migration.wren` or be
inside the `_app` folder, `_app/migration.wren`.

This script will be run every time the application starts and also when a Wren
file is updated. For a complete migration example in the context of a REST API,
see the [Building REST APIs](rest-api.md) guide.

```wren

Db.migrate("Name of the migration", `SOME QUERY`)
```

The name of the migration is used to avoid repeating migrations. Use a
descriptive name.

You can have multiples queries separated by `;`.

Use migration to insert non-transactional data. You can interact with the
`BIALET_*` tables.

```wren
Db.migrate("Add default title", `INSERT INTO BIALET_CONFIG VALUES ('title', 'Bialet example page')`)
```

## Bialet tables

Bialet tables are prefixed with `BIALET_`.

- `BIALET_CONFIG`: The configuration table.
- `BIALET_MIGRATIONS`: The migration history table.
- `BIALET_SESSIONS`: The session table.
- `BIALET_FILES`: The file storage table.
- `BIALET_LOGS`: The logging table.
- `BIALET_REMOTE_MODULES`: The remote module cache table.

If you delete or alter any of these tables your application will not work
correctly.

You may insert, update or delete rows, however do it with caution.
