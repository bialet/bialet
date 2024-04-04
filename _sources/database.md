# Database

Bialet includes the [SQLite](https://www.sqlite.org/) library for its database.

## Configuration

There is no configuration other than naming the database file.

The default database name is `_db.sqlite3` placed in the root directory.

To change the name of the database, set the `-d` option.

**There is no integration with other databases at the moment.**

## Query Object

Use backticks to surround SQL statements `` ` ` ``.

The Query object have 3 main methods all accept the same parameters.

* `query()`: Returns the last inserted id if the query was a `INSERT` statement.
* `fetch()`: Returns the result of the query as an array (List object).
* `first()`: Returns the first result of the query as an object (Map object).

**First will add a LIMIT clause automatically.**

You can't concatenate strings or use interpolations with the Query object.
When you need to add parameters, use placeholders `?` and send the parameters to the method.

All methods receive the following parameters:

* `()`: No parameters.
* `(params)`: An array of parameters.
* `(param1)`: Any non array parameter is converted to String.
* `(param1, param2)`: Convert all parameters to String.
* `(param1, param2, param3)`: Convert all parameters to String.

If you need more parameters, use the array syntax (`([param1, param2, param3, param4, param5, ...])`).

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
```

## Insert and update

You can use a regular SQL `INSERT` or `UPDATE` statement.

```wren
var userParams = ["John Doe", "john@example.com"]
var id = `INSERT INTO users (name, email) VALUES (?, ?)`.query(userParams)
```

But you can also use the `Db.save` method, sending the table name and the a Map object with the values.

```wren
import "bialet" for Db
var user = {"name": "John Doe", "email": "john@example.com"}
var id = Db.save("users", user)
```

The same method also work for updating the row.

```wren
import "bialet" for Db
var user = {"name": "John Doe", "email": "john@example.com"}
user["id"] = 1
// This will update the row
Db.save("users", user)
```

## Migrations

The migration file can be in the root and be called `_migration.wren` or be inside the `_app` folder, `_app/migration.wren`.

This script will be run every time the application starts and also when a Wren file is updated.

```wren
import "bialet" for Db

Db.migrate("Name of the migration", `SOME QUERY`)
```

The name of the migration is used to avoid repeating migrations. Use a descriptive name.

You can have multiples queries separated by `;`.

Use migration to insert non-transactional data. You can interact with the `BIALET_*` tables.

```wren
Db.migrate("Add default title", `INSERT INTO BIALET_CONFIG VALUES ("title", "Bialet example page")`)
```

## Bialet tables

Bialet tables are prefixed with `BIALET_`.

* `BIALET_CONFIG`: The configuration table.
* `BIALET_MIGRATIONS`: The migration history table.
* `BIALET_SESSIONS`: The session table.

If you delete or alter any of this tables your application will not work correctly.

You may insert, update or delete rows, however do it with caution.
