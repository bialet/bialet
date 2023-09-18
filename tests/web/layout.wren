import "menu" for Menu

class Layout {
  static render(title, content){ "<html>
    <head>
      <title>%( title )</title>
      <style>
        html{color:#243453;background:#F5F5F5;max-width:80\%;max-width:80ch;padding:calc(1vmin + .5rem);margin:0 auto;font:14pt sans-serif;font-size:clamp(1.3rem,2.5vw,1.6rem);font-family:system-ui;line-height:1.5}pre{background:#fef;padding:1em;overflow:auto}img{display:block;width:100\%}nav ul{padding:0;display:flex;justify-content:space-between;flex-flow:row wrap;gap:1em}nav li{list-style-type:none}footer{margin:2em 0;border-top:.3em solid #ccc}a{color:#93255F}h2{font-weight:400;margin-top:1.75em}
      </style>
    </head>
    <body>
      <h1 style='text-align:center'>%( title )</h1>
      %( Menu.render() )
      <main>
        %( content )
      </main>
    </body>
  </html>" }
}
