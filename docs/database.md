# Database

Bialet includes the [SQLite](https://www.sqlite.org/) library for its database.

## Configuration

There is no configuration other than naming the database file.

The default database name is `_db.sqlite3` placed in the root directory.

To change the name of the database, set the `-d` option.

**There is no integration with other databases at the moment.**

## Query Object

Use backticks to surround SQL statements `` ` ` ``.

The Query object have 5 methods. All the methods accept the same parameters.

- `query()`: Returns the last inserted id if the query was a `INSERT` statement.
- `fetch()`: Returns the result of the query as an array (List object).
- `first()`: Returns the first result of the query as an object (Map object).
- `val()`: Returns the value of the first result.
- `toNum()`: Returns the value of the first result as a number.

**First will add a LIMIT clause automatically in first(), val and toNum.**

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

This method is especially useful for REST APIs where sort parameters come from user input, preventing SQL injection through column name manipulation.

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

## Insert and update

You can use a regular SQL `INSERT` or `UPDATE` statement.

```wren
var userParams = ["John Doe", "john@example.com"]
var id = `INSERT INTO users (name, email) VALUES (?, ?)`.query(userParams)
```

But you can also use the `Db.save` method, sending the table name and the a Map
object with the values.

```wren
var user = {"name": "John Doe", "email": "john@example.com"}
var id = Db.save("users", user)
```

The same method also work for updating the row.

```wren
var user = {"name": "John Doe", "email": "john@example.com"}
user["id"] = 1
// This will update the row
Db.save("users", user)
```

## Data Types and BLOB Support

SQLite supports several data types including TEXT, INTEGER, REAL, BLOB, and
NULL. Bialet handles most of these types automatically:

- **TEXT**: Returned as strings
- **INTEGER** and **REAL**: Returned as numbers
- **NULL**: Returned as null values

**Note on BLOB data**: While BLOB (Binary Large Object) data is retrieved
correctly from SQLite, it may not be properly handled when passed to Wren code
due to Wren's string representation. If you need to work with binary data,
consider encoding it as base64 or using another text-based representation.

## Migrations

The migration file can be in the root and be called `_migration.wren` or be
inside the `_app` folder, `_app/migration.wren`.

This script will be run every time the application starts and also when a Wren
file is updated.

```wren

Db.migrate("Name of the migration", `SOME QUERY`)
```

The name of the migration is used to avoid repeating migrations. Use a
descriptive name.

You can have multiples queries separated by `;`.

Use migration to insert non-transactional data. You can interact with the
`BIALET_*` tables.

```wren
Db.migrate("Add default title", `INSERT INTO BIALET_CONFIG VALUES ("title", "Bialet example page")`)
```

## Bialet tables

Bialet tables are prefixed with `BIALET_`.

- `BIALET_CONFIG`: The configuration table.
- `BIALET_MIGRATIONS`: The migration history table.
- `BIALET_SESSIONS`: The session table.

If you delete or alter any of this tables your application will not work
correctly.

You may insert, update or delete rows, however do it with caution.
