Test.get("/login-check")
  .status(401)

Test.get("/login-check")
  .setHeader("Authorization", "Basic YWRtaW46d3Jvbmc=")
  .status(401)

Test.get("/login-check")
  .setHeader("Authorization", "Basic YWRtaW46c2VjcmV0")
  .status(200)
  .equals("authenticated")

Test.get("/login-check")
  .setHeader("Authorization", "Basic bm90YmFzaWM=")
  .status(401)

// A truncated or impossible Base64 value is a client error: decodeBase64 used
// to index past the end of the value and abort the request with a 500.
Test.get("/login-check")
  .setHeader("Authorization", "Basic YWJj")
  .status(401)

Test.get("/login-check")
  .setHeader("Authorization", "Basic YQ")
  .status(401)

Test.get("/login-check")
  .setHeader("Authorization", "Basic !!!!")
  .status(401)

// Padding in a non-final group is not valid Base64.
Test.get("/login-check")
  .setHeader("Authorization", "Basic YQ==YWJj")
  .status(401)

// An attacker-chosen credential length is refused before it is decoded.
Test.get("/login-check")
  .setHeader("Authorization", "Basic " + ("A" * 5000))
  .status(401)
