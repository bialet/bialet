class TodoTemplate {
  static list=(list){ __list = list }
  static title { "TODO List" }
  static layout { "
    <html>
      <head>
        <title>%( title )</title>
        <meta charset='utf-8' />
        <meta name='viewport' content='width=device-width, initial-scale=1.0' />
        <link rel='stylesheet' href='/style.css' />
      </head>
      <body>
        <h1>%( title )</h1>
        %( listItems )
        %( newItemForm )
        %( clearForm )
      </body>
    </html>
  " }
  static listItems { __list.count > 0 ? "
    <ul>
      %( __list.map{|task| "
        <li class='finished_%( task["finished"] )'>
          <a href='/toggle?id=%( task["id"] )'>%( task["description"] )</a>
        </li>
      "})
    </ul>
  " : "<p class='no-tasks'>No tasks yet</p>" }
  static newItemForm { "
    <form method='post'>
      <p>
        <input name='task' placeholder='New task' required />
        <button>Create</button>
      </p>
    </form>
  " }
  static clearForm { "
    <form method='post' action='/clear'>
      <p><button>Clear finished tasks</button></p>
    </form>
  " }
}
