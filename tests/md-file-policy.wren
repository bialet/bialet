var json = Markdown.file("ignored/data.json")
var route = Markdown.file("get.wren")
var md = Markdown.file("test-markdown.md")
return "%(json == false)|%(route == false)|%(md != false)"
