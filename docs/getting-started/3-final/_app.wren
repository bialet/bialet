class Template {
  static layout(content) { <!doctype html>
<html>
  <head class="h-full dark:bg-gray-100">
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Poll</title>
    <link href="https://cdnjs.cloudflare.com/ajax/libs/flowbite/2.3.0/flowbite.min.css" rel="stylesheet" />
  </head>
  <body class="h-full dark overflow-hidden">
    <div class="h-screen md:container md:mx-auto bg-white dark:bg-gray-900 px-4">
      {{ header }}
      <main>
        <div class="mx-auto max-w-7xl py-6 sm:px-6 lg:px-8">
          {{ content }}
        </div>
      </main>
      {{ footer }}
    </div>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/flowbite/2.3.0/flowbite.min.js"></script>
  </body>
</html> }

  static header { <header>
        <div class="py-8 px-4 mx-auto max-w-screen-xl text-center lg:py-16">
          <h1><a href="." class="mb-4 text-4xl font-extrabold tracking-tight leading-none text-gray-900 md:text-5xl lg:text-6xl dark:text-white">Poll</a></h1>
        </div>
      </header> }

  static footer {
    // This is a multiline method declaration, that means we need the `return`
    return <footer class="py-8 px-4 mx-auto max-w-screen-xl text-center lg:py-16 text-gray-500">
        Made with <a href="https://bialet.dev" class="text-blue-600 hover:underline dark:text-blue-500">Bialet</a>.
        View <a href="https://github.com/bialet/bialet/tree/main/docs/getting-started" class="text-blue-600 hover:underline dark:text-blue-500">source code</a>.
    </footer>
    // Also we need a new line here
  }
}

class Poll {
  construct new() {
    _opts = null
    _total = null
  }
  // Fetch the options from the database into the `_opts` property.
  // Queries are part of Bialet.
  options { _opts || (_opts = `SELECT * FROM poll`.fetch()) }
  // Add parameters to the query like a prepared statement.
  vote(opt) { `UPDATE poll SET votes = votes + 1
               WHERE id = ?`.query(opt) }
  // Getter to get the total number of votes.
  totalVotes { _total || (_total = options.reduce(0, Fn.new{|sum, opt| sum + votes_(opt) }))}
  // Calculate the percentage of votes for an option
  percentage(opt) { totalVotes > 0 ? ((votes_(opt) / totalVotes) * 100).round : 0 }
  // Use the method to get the votes as a number.
  // There is no access modifier in Wren. Add an underscore at the end
  // of the method name to identify it as private.
  votes_(opt) { Num.fromString(opt["votes"]) }
}
