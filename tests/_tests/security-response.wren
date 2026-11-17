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

// The server frames every response itself; an app value for a framing field
// would give one response two framings.
for (name in ["Content-Length", "transfer-encoding", "Connection"]) {
  var framing = Fiber.new { Response.header(name, "0") }
  framing.try()
  Test.assert(framing.error != null, "%(name) cannot be set by the app")
}
var spaced = Fiber.new { Response.header("X Spaced", "v") }
spaced.try()
Test.assert(spaced.error != null, "a header name with a space is refused")
