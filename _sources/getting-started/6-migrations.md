# 6. Migrations

Manually creating tables with an external tool works, but Bialet provides
**migrations** — versioned SQL that runs automatically on startup.

Create `_migration.wren` in your project root:

```wren
Db.migrate("Create Poll table", `
  CREATE TABLE poll (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    answer TEXT NOT NULL,
    comment TEXT,
    votes INTEGER NOT NULL DEFAULT 0
  )
`)

Db.migrate("Add initial data", `
  INSERT INTO poll (answer, comment) VALUES
    ("Yes", "We need to go back to the good old times"),
    ("No", "You are just old")
`)
```

- Each `Db.migrate()` call has a **name** and a **SQL query**
- Bialet tracks which migrations have run (stored in the database itself)
- Migrations run once — the name prevents re-execution
- They run on every server start, so adding new ones is safe

Migrations are the recommended way to manage your schema. They keep your
database in sync with your code and make it easy to share your project —
anyone who runs it gets the same schema automatically.

In the next chapter, we'll connect the migration-defined table to our form
and voting logic.

---
**Previous:** [5. Logic & Database](5-logic-database) &nbsp; | &nbsp; **Next:** [7. Forms & Voting](7-forms-voting)
