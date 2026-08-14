var show = true
var x = <div>{{
  show &&
  <form method="post" action="/clear">
    <button>Clear</button>
  </form>
}}</div>
System.print(x)
