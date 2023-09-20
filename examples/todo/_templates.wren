class TodoTemplate {
  static list=(list){ __list = list }
  static title { "Wren TODO List" }
  static layout { "
    <html>
      <head>
        <title>%( title )</title>
      </head>
      <body>
        <h1>%( title )</h1>
        %( listItems )
        %( newItemForm )
      </body>
    </html>
  " }
  static listItems { "
    <ul>
      %( __list.map{|task| "
        <li>%( task["description"] )</li>
      "})
    </ul>
  " }
  static newItemForm { "
    <form method='post'>
      <input name='task' />
      <submit>Create</submit>
    </form>
  " }
}

