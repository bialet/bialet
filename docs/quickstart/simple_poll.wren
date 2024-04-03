import "bialet" for Request, Response

// This makes the example self-contained and runable.
// Don't forget to use migrations in your apps.
`CREATE TABLE IF NOT EXISTS simple_poll (answer TEXT PRIMARY KEY, votes INT)`.query()
`INSERT OR IGNORE INTO simple_poll VALUES ("Yes", 0)`.query()
`INSERT OR IGNORE INTO simple_poll VALUES ("No", 0)`.query()

var vote

if (Request.isPost) {
  vote = Request.post("vote")
  `UPDATE simple_poll SET votes = votes + 1 WHERE answer = ?`.query(vote)
  System.print("Voted for %(vote)")
}

var options = `SELECT * FROM simple_poll`.fetch()
System.print(options)

Response.out('
<html>
  <body>
    <h1>Has web development become overly complex?</h1>

    %( !vote ? '

      <form method="post">
        %( options.map{ |opt| '
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
        %( options.map{|opt| '<li>%(opt["answer"]) - %(opt["votes"]) votes</li>' })
      </ul>
      <p><a href="">Vote again</a></p>

    ')
  </body>
</html>
')
