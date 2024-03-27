# 🚲 bialet

**[Bialet](https://bialet.dev) is a full-stack web framework that integrates the object-oriented Wren language with a single HTTP server and a built-in database, creating a unified environment for web development**

<p align="center">
  <img src="https://github.com/bialet/bialet/assets/142173/af827692-0e0d-4805-a478-77d07bd62e18" alt="Make Web Development Great Again hat" width="200" />
</p>
<p align="center">
  <strong>Make Web Development Great Again</strong>
</p>

## Quickstart

1. Install Bialet using Docker Compose.

```bash
git clone https://github.com/bialet/bialet.git
cd bialet
docker compose up
```

2. Visit [localhost:7000](http://localhost:7000) in your browser.

See [installation](docs/source/installation.md) for details on building and running the project.

## A single file web application example

This example shows how to build a simple Poll web application using Bialet.


```sql
CREATE TABLE poll (answer TEXT PRIMARY KEY, votes INT);
INSERT INTO poll VALUES ("Yes", 0), ("No", 0);
```

```wren
// Imports the Request and Response classes for managing HTTP interactions.
import "bialet" for Request, Response

// Define a constant to avoid duplication in the template.
var QUESTION = "Has web development become overly complex?"

// Let's group the domain logic in a class.
class Poll {
  construct new() { _opts = null }
  // Fetch the options from the database and store them in the `_opts` property.
  // The query is part of the language and they are actually prepared statements.
  options { _opts || (_opts = `SELECT * FROM poll`.fetch()) }
  totalVotes { options.reduce(0, Fn.new{|sum, opt| sum + Num.fromString(opt["votes"]) })}
  vote(opt) { `UPDATE poll SET votes = votes + 1 WHERE answer = ?`.query(opt) }
}

var poll = Poll.new()
var showResults = false

// Handle the form submission
if (Request.isPost) {
  var votedOpt = Request.post("vote")
  poll.vote(votedOpt)
  showResults = true
}

// Now we can render the HTML.
// All the template logic is do it with string interpolation `%( ... )`.
// Use the ternary operator `?` to conditionally render HTML.
// Also use `map` to iterate over an array.
Response.out('
<!DOCTYPE html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="style.css">
    <title>%(QUESTION)</title>
  </head>
  <body>
    <h1>%(QUESTION)</h1>

    %( !showResults ? '

      <!-- Form for voting -->
      <form method="post">
        %( poll.options.map{ |opt| '
          <p>
            <label>
              <input type="radio" name="vote" value="%(opt["answer"])">
              %(opt["answer"])
            </label>
          </p>
        ' } )
        <p><input type="submit" value="Vote"></p>
      </form>

    ' : '

      <!-- Show results -->
      <p>Thank you for voting!</p>
      <ul>
        %( poll.options.map{|opt| '<li>%(opt["answer"]) - %(opt["votes"]) votes</li>' })
      </ul>
      <p>Total votes: <strong>%( poll.totalVotes )</strong></p>

    ')
  </body>
</html>
')
```

See more examples in the [documentation](https://bialet.dev).

## License

Bialet is released under the GPL 2 license, ensuring that it remains free and open-source, allowing users to modify and share the software under the same license terms.

## Credits

~I copy a lot of code from all over the web~
Bialet incorporates the work of several open-source projects and contributors. We extend our gratitude to:

- The [Wren programming language](https://wren.io), for its lightweight, flexible, and high-performance capabilities.
- The [Mongoose library](https://github.com/expressjs/mongoose), for providing an easy-to-use web server solution.
- Matthew Brandly, for his invaluable contributions to JSON parsing and utility functions in Wren. Check out his work at [Matthew Brandly's GitHub](https://github.com/brandly/wren-json).
- @PureFox48 for the upper and lower functions.
- [Codeium](https://github.com/codeium) for the [Codeium](https://codeium.com) plugin, ChatGPT and a lot of coffee.

We encourage users to explore these projects and recognize the efforts of their creators.
