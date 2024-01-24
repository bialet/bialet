import "bialet" for Html

class Template {

  construct new(){ _title = "TODO tasks" }

  home(tasks) { '
    <html>
      <head>
        <title>%( _title )</title>
        <meta charset="utf-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <link rel="stylesheet" href="/style.css" />
      </head>
      <body>
        <h1>%( _title )</h1>
        %( tasks.count > 0 ? '
          <ul>
            %( tasks.map{ |task| '
              <li class="finished_%( task["finished"] )">
                <a href="/toggle?id=%( task["id"] )">
                  %( Html.escape(task["description"]) )
                </a>
              </li>
            '})
          </ul>
        ' : '
          <p class="no-tasks">No tasks yet</p>
        ' )
        %( newItemForm )
        %( clearForm )
      </body>
    </html>
  ' }

  newItemForm { '
    <form method="post">
      <p>
        <input name="task" placeholder="New task" required />
        <button>Create</button>
      </p>
    </form>
  ' }

  clearForm { '
    <form method="post" action="/clear">
      <p><button>Clear finished tasks</button></p>
    </form>
  ' }
}
