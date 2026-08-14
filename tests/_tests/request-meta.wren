Test.get("/request-meta?foo=bar")
  .status(200)
  .equals("get|/request-meta|bar")

Test.get("/request-meta")
  .status(200)
  .equals("get|/request-meta|null")

Test.post("/request-meta", {"foo": "bar"})
  .status(200)
  .equals("form|/request-meta|foo=bar|application/x-www-form-urlencoded")

Test.apiPost("/request-meta", {"key": "jsonvalue"})
  .status(200)
  .equals("json|/request-meta|jsonvalue")

Test.get("/request-meta?mode=header")
  .setHeader("X-Custom", "hello-header")
  .status(200)
  .equals("hello-header")

Test.get("/request-meta?mode=header")
  .status(200)
  .equals("none")
