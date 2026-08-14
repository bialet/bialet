Test.get("/response-errors?forbidden=1")
  .status(403)

Test.get("/response-errors?notfound=1")
  .status(404)

Test.get("/response-page")
  .status(200)
  .contains("Page&lt;title&gt;")
  .contains("Hello &amp; welcome")

Test.get("/response-out")
  .status(200)
  .contains("first\r\nsecond")
  .contains("status:200")

Test.get("/response-headers")
  .status(200)
  .headerContains("Set-Cookie", "tracker=1")
  .headerContains("X-Custom", "custom-value")

Test.get("/cookie-delete")
  .status(200)
  .contains("present:empty|fallback:fallback-value|missing:null")
