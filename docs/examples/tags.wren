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

if (a<b || b>a) {
  System.print(title)
}
Response.out(<html><body>{{ content }}</body></html>)
