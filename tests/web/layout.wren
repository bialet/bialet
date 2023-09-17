import "menu" for Menu

class Layout {
  static render(title, content){ "<html>
    <head>
      <title>%( title )</title>
      <style>
        body { font-family: system-ui; }
      </style>
    </head>
    <body>
      <h1>%( title )</h1>
      %( Menu.render() )
      <main>
        %( content )
      </main>
    </body>
  </html>" }
}
