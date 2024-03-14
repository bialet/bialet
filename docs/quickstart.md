# Quickstart

```wren
  import "bialet" for Request, Response

  var QUESTION = "Has web development become overly complex?"

  class Poll {
    construct new() { _opts = null }
    // Queries are integrated with Wren. They are objects.
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

      ')
      <p><a href=".">Back ↩️</a></p>
    </body>
  </html>
  ')
```

