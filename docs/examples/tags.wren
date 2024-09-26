import "bialet" for Request, Response

var title = <h1>Example</h1>
var name = "John"
var a = Num.fromString(Request.get("a") || "1")
var b = Num.fromString(Request.get("b") || "2")
var list = ["One","Two","Three","Four"]
var hello = <p>Hello {{ name }}</p>
var content = <div class="container">
  {{ title }}
  {{ hello }}
  <p>List:</p>
  <ul>
  {{ list.map{|x| <li>{{ x }}</li> } }}
  </ul>
  <p>Use {{ "{{" }} to start a block and {{ "}}" }} to end a block</p>

  <p>{{ a < b && <b>A is less than B</b> }}</p>
</div>

// Self-closing tags
var line = <hr />
var charset = "utf-8"
var metaTag = <meta charset="{{ charset }}" />
var input = <input type="{{ "text" }}" name="name" value="{{ name }}" />

// Yes, < and > signs still work!
if (a<b || b>a) {
  System.print(title)
}

// Doctype is a special case, it will end on a close HTML tag: `</html>`
Response.out(<!doctype html>
<html>
<head>
  {{ metaTag }}
</head>
<body>
  {{ content }}
  {{ line }}
  {{ input }}
</body>
</html>)
