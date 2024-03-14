// Imports the Request and Response classes for managing HTTP interactions.
import "bialet" for Request, Response

// This makes the example self-contained and runable.
`CREATE TABLE IF NOT EXISTS poll (answer TEXT PRIMARY KEY, votes INT)`.query()
`INSERT OR IGNORE INTO poll VALUES ("Yes", 0)`.query()
`INSERT OR IGNORE INTO poll VALUES ("No", 0)`.query()

// Let's group the domain logic in a class.
class Poll {
  construct new() { _opts = null }
  // Fetch the options from the database into the `_opts` property.
  // Queries are part of Bialet.
  options { _opts || (_opts = `SELECT * FROM poll`.fetch()) }
  // Add parameters to the query like a prepared statement.
  vote(opt) { `UPDATE poll SET votes = votes + 1
               WHERE answer = ?`.query(opt) }
  // Getter to get the total number of votes.
  totalVotes { options.reduce(0, Fn.new{|sum, opt| sum + votes_(opt) })}
  // Use the method to get the votes as a number.
  // There is no access modifier in Wren. Add an underscore at the end
  // of the method name to identify it as private.
  votes_(opt) { Num.fromString(opt["votes"]) }
}

var poll = Poll.new()
var showResults = false

// Handle the form submission
if (Request.isPost) {
  var votedOpt = Request.post("vote")
  poll.vote(votedOpt)
  showResults = true
}

// Define a constant to avoid duplication in the template.
var QUESTION = "Has web development become overly complex?"

// Now we can render the HTML.
// All the template logic is do it with string interpolation `%( ... )`.
// Use the ternary operator `?` to conditionally render HTML.
// Also use `map` to iterate over an array.
Response.out('
<!DOCTYPE html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>%(QUESTION)</title>
  </head>
  <body style="margin:4em auto;max-width:600px">
    <h1 style="text-align:center">%(QUESTION)</h1>

    %( !showResults ? '

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

      <p>Thank you for voting!</p>
      <ul>
        %( poll.options.map{|opt| '<li>%(opt["answer"]) - %(opt["votes"]) votes</li>' })
      </ul>
      <p>Total votes: <strong>%( poll.totalVotes )</strong></p>
      <p><a href="">Vote again</a></p>

    ')
  </body>
</html>
')
