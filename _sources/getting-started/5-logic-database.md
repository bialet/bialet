# 5. Logic & Database

Before integrating with our full app, let's build a standalone version to
understand the core patterns.

## Create the database table

Bialet uses SQLite. When you run the server, a `_db.sqlite3` file is created
automatically. Connect to it with [SQLite
Browser](https://sqlitebrowser.org/) or the [SQLite
CLI](https://sqlite.org/cli.html) and run:

```sql
CREATE TABLE simple_poll (answer TEXT PRIMARY KEY, votes INT);
INSERT INTO simple_poll VALUES ("Yes", 0);
INSERT INTO simple_poll VALUES ("No", 0);
```

## Query and display

Create `simple_poll.wren`:

```wren
var options = `SELECT * FROM simple_poll`.fetch()

return <html>
  <body>
    <h1>Has web development become overly complex?</h1>
    <form method="post">
      {{ options.map {|opt| <p>
        <label>
          <input type="radio" name="vote" value="{{ opt["answer"] }}">
          {{ opt["answer"] }}
        </label>
      </p> } }}
      <p><input type="submit" value="Vote"></p>
    </form>
  </body>
</html>
```

The backtick string `` `SELECT * FROM simple_poll` `` is a **Query object**
— a prepared statement. `.fetch()` executes it and returns all rows as a
list of maps.

## Handle POST

Add vote handling above the query:

```wren
if (Request.isPost) {
  var vote = Request.post("vote")
  `UPDATE simple_poll SET votes = votes + 1 WHERE answer = ?`.query(vote)
  System.log("Voted for %(vote)")
}

var options = `SELECT * FROM simple_poll`.fetch()
```

- `Request.isPost` checks if the form was submitted
- `Request.post("vote")` reads the form field
- `.query(vote)` executes a parameterized UPDATE
- `?` placeholders prevent SQL injection — never concatenate user input

`System.log()` prints to the server terminal — useful for debugging.

## Show results after voting

Use a ternary to switch between the form and the results view:

```wren
var vote
if (Request.isPost) {
  vote = Request.post("vote")
  `UPDATE simple_poll SET votes = votes + 1 WHERE answer = ?`.query(vote)
}

var options = `SELECT * FROM simple_poll`.fetch()

return <html>
  <body>
    <h1>Has web development become overly complex?</h1>
    {{ !vote ?
      <form method="post">
        {{ options.map {|opt| <p>
          <label>
            <input type="radio" name="vote" value="{{ opt["answer"] }}">
            {{ opt["answer"] }}
          </label>
        </p> } }}
        <p><input type="submit" value="Vote"></p>
      </form>
    :
      <main>
        <p>Thank you for voting!</p>
        <ul>
          {{ options.map {|opt|
            <li>{{ opt["answer"] }} - {{ opt["votes"] }} votes</li>
          } }}
        </ul>
        <p><a href="">Vote again</a></p>
      </main>
    }}
  </body>
</html>
```

The pattern `{{ condition ? <true-branch> : <false-branch> }}` is a ternary
— the Wren equivalent of if/else inline.

For a deeper dive into queries, see [Database](../database.md).

---
**Previous:** [4. Templates](4-templates) &nbsp; | &nbsp; **Next:** [6. Migrations](6-migrations)
