var redirectTarget = "http://example.com/?q=<script>alert(1)</script>"
var redirectResponse = Test.get("/security-response?" + Util.params({
  "mode": "redirect",
  "to": redirectTarget
}))
redirectResponse
  .status(302)
  .header("Location", redirectTarget)
  .contains("&lt;script&gt;alert(1)&lt;/script&gt;")
  .notContains("<script>alert(1)</script>")

Test.get("/security-response?" + Util.params({
  "mode": "end",
  "title": "<img src=x onerror=alert(1)>",
  "message": "<script>alert(1)</script>"
}))
  .status(418)
  .contains("&lt;img src=x onerror=alert(1)&gt;")
  .contains("&lt;script&gt;alert(1)&lt;/script&gt;")
  .notContains("<script>alert(1)</script>")

var percent = String.fromByte(37)
var poisonedLocation = "http://example.com/" + percent + "0D" + percent + "0AX-Injected:" + percent + "20yes"

Test.get("/security-response?mode=redirect&to=" + poisonedLocation)
  .status(302)
  .header("Location", "http://example.com/X-Injected: yes")
